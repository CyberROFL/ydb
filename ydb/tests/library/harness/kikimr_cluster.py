#!/usr/bin/env python
# -*- coding: utf-8 -*-
import itertools
import logging
import time
from concurrent import futures

from .kikimr_runner import KiKiMR, KikimrExternalNode
from .kikimr_cluster_interface import KiKiMRClusterInterface
import yaml

logger = logging.getLogger(__name__)

DEFAULT_INTERCONNECT_PORT = 19001
DEFAULT_MBUS_PORT = 2134
DEFAULT_MON_PORT = 8765
DEFAULT_GRPC_PORT = 2135


def kikimr_cluster_factory(configurator=None, config_path=None):
    # TODO: remove current function, use explicit KiKiMR/ExternalKiKiMRCluster
    logger.info("Starting standalone YDB cluster")
    if config_path is not None:
        return ExternalKiKiMRCluster(config_path)
    else:
        return KiKiMR(configurator)


class ExternalKiKiMRCluster(KiKiMRClusterInterface):
    def __init__(
            self,
            cluster_template,
            kikimr_configure_binary_path,
            kikimr_path,
            kikimr_next_path=None,
            ssh_username=None,
            deploy_cluster=False,
            yaml_config=None):

        with open(cluster_template, 'r') as r:
            self.__cluster_template = yaml.safe_load(r.read())
        self.__yaml_config = None
        if yaml_config is not None:
            with open(yaml_config, 'r') as r:
                self.__yaml_config = yaml.safe_load(r.read())
        self.__kikimr_configure_binary_path = kikimr_configure_binary_path
        self._slots = None
        self.__kikimr_path = kikimr_path
        self.__kikimr_next_path = kikimr_next_path
        self.__ssh_username = ssh_username
        self.__deploy_cluster = deploy_cluster

        if yaml_config is not None:
            self.__hosts = self.__yaml_config.get('config', {}).get('hosts')
        else:
            # Backward compatibility for cluster_template
            self.__hosts = self.__cluster_template.get('hosts')

        self.__slot_count = 0
        for domain in self.__cluster_template['domains']:
            self.__slot_count = max(self.__slot_count, domain['dynamic_slots'])

        super(ExternalKiKiMRCluster, self).__init__()

    @property
    def config(self):
        return self.__cluster_template

    @property
    def yaml_config(self):
        return self.__yaml_config

    def add_storage_pool(self, erasure=None):
        raise NotImplementedError()

    def start(self):
        self._prepare_cluster()

        self._start()
        return self

    def stop(self):
        return self

    def restart(self):
        self._stop()
        self._start()
        return self

    def cleanup_logs(self):
        for inst_set in [self.nodes, self.slots]:
            self._run_on(
                inst_set,
                lambda x: x.cleanup_logs()
            )

    def _start(self):
        for inst_set in [self.nodes, self.slots]:
            self._run_on(inst_set, lambda x: x.start())

    def _stop(self):
        for inst_set in [self.nodes, self.slots]:
            self._run_on(inst_set, lambda x: x.stop())

    @staticmethod
    def _run_on(instances, *funcs):
        with futures.ThreadPoolExecutor(8) as executor:
            for func in funcs:
                results = executor.map(
                    func,
                    instances.values()
                )
                # raising exceptions here if they occured
                # also ensure that funcs[0] finished before func[1]
                for _ in results:
                    pass

    def _rename_obsolete_files(self):
        obsolete_files = [
            "/Berkanavt/kikimr/secrets/tvm_secret",
            "/Berkanavt/kikimr/secrets/auth.txt",
            "/Berkanavt/kikimr/cfg/auth.txt",
        ]
        for path in obsolete_files:
            self._run_on(
                self.nodes,
                lambda x: x.ssh_command(
                    "sudo mv {path} {path}.delete_me".format(path=path)
                )
            )

    def _initialize(self):
        node = list(self.nodes.values())[0]
        # Initilize, these scripts are generated by ydb/tools/cfg
        # TODO: do something like "ydb admin config replace -f config.yaml" instead
        node.ssh_command(['bash /Berkanavt/kikimr/cfg/init_storage.bash'], raise_on_error=True)
        node.ssh_command(['bash /Berkanavt/kikimr/cfg/init_cms.bash'], raise_on_error=True)
        node.ssh_command(['bash /Berkanavt/kikimr/cfg/init_compute.bash'], raise_on_error=True)
        node.ssh_command(['bash /Berkanavt/kikimr/cfg/init_databases.bash'])

    def _prepare_cluster(self):
        self._stop()
        self._rename_obsolete_files()

        for inst_set in [self.nodes]:
            self._run_on(
                inst_set,
                lambda x: x.prepare_artifacts(
                    self.__cluster_template
                )
            )

        # enctyption key for pdisk
        self._run_on(
            self.nodes,
            lambda x: x.ssh_command(
                'echo "Keys { ContainerPath: \'"\'/Berkanavt/kikimr/cfg/fake-secret.txt\'"\' Pin: \'"\' \'"\' Id: \'"\'fake-secret\'"\' Version: 1 }" | '
                'sudo tee /Berkanavt/kikimr/cfg/key.txt',
            )
        )

        self._run_on(
            self.nodes,
            lambda x: x.ssh_command(
                "echo \"simple text\" | "
                "sudo tee /Berkanavt/kikimr/cfg/fake-secret.txt"
            )
        )

        self._run_on(
            self.nodes,
            lambda x: x.ssh_command(
                "sudo chown root:root /Berkanavt/kikimr/cfg/key.txt && sudo chown root:root /Berkanavt/kikimr/cfg/fake-secret.txt"
            )
        )

        if self.__deploy_cluster:
            for inst_set in [self.nodes, self.slots]:
                self._run_on(
                    inst_set,
                    lambda x: x.cleanup_logs()
                )

            self._run_on(
                self.nodes,
                lambda x: x.cleanup_disks(),
                lambda x: x.start()
            )

            time.sleep(5)

            self._initialize()

        self._start()

    def _get_bridge_pile_id(self, node):
        if self.__yaml_config is None:
            return None

        bridge_config = self.__yaml_config.get('config', {}).get('bridge_config', None)
        if bridge_config is None:
            return None

        bridge_pile_name = node.get('location', {}).get('bridge_pile_name', None)
        if bridge_pile_name is None:
            return None

        for pile_id, pile in enumerate(bridge_config.get('piles', [])):
            if pile.get('name') == bridge_pile_name:
                return pile_id
        return None

    @property
    def nodes(self):
        return {
            node_id: KikimrExternalNode(
                kikimr_configure_binary_path=self.__kikimr_configure_binary_path,
                kikimr_path=self.__kikimr_path,
                kikimr_next_path=self.__kikimr_next_path,
                node_id=node_id,
                host=node.get('name', node.get('host')),
                rack=node.get('location', {}).get('rack', None),
                datacenter=node.get('location', {}).get('data_center', None),
                bridge_pile_name=node.get('location', {}).get('bridge_pile_name', None),
                bridge_pile_id=self._get_bridge_pile_id(node),
                ssh_username=self.__ssh_username,
                port=DEFAULT_GRPC_PORT,
                mon_port=DEFAULT_MON_PORT,
                ic_port=DEFAULT_INTERCONNECT_PORT,
                mbus_port=DEFAULT_MBUS_PORT,
                configurator=None,
            ) for node_id, node in zip(itertools.count(start=1), self.__hosts)
        }

    @property
    def slots(self):
        if self._slots is None:
            self._slots = {}
            slot_count = self.__slot_count
            slot_idx_allocator = itertools.count(start=1)
            for node_id in sorted(self.nodes.keys()):
                node = self.nodes[node_id]
                start = 31000
                for node_slot_id in range(1, slot_count + 1):
                    slot_idx = next(slot_idx_allocator)
                    mbus_port = start + 0
                    grpc_port = start + 1
                    mon_port = start + 2
                    ic_port = start + 3

                    self._slots[slot_idx] = KikimrExternalNode(
                        kikimr_configure_binary_path=self.__kikimr_configure_binary_path,
                        kikimr_path=self.__kikimr_path,
                        kikimr_next_path=self.__kikimr_next_path,
                        node_id=node_id,
                        host=node.host,
                        rack=node.rack,
                        datacenter=node.datacenter,
                        bridge_pile_name=node.bridge_pile_name,
                        bridge_pile_id=node.bridge_pile_id,
                        ssh_username=self.__ssh_username,
                        port=grpc_port,
                        mon_port=mon_port,
                        ic_port=ic_port,
                        mbus_port=mbus_port,
                        configurator=None,
                        slot_id=node_slot_id,
                    )
                    start += 10

        return self._slots

    def _get_node(self, host, grpc_port):
        nodes = self._slots if self._slots else self.nodes
        for node in nodes.values():
            if node.host == host and str(node.grpc_port) == str(grpc_port):
                return node
        logger.error('Cant find node with host {} and grpc_port {}'.format(host, grpc_port))
