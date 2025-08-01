#pragma once

#include "defs.h"
#include "pdiskid.h"
#include "pdisk_state.h"
#include "pdisk_status.h"

#include <util/generic/hash.h>
#include <util/generic/hash_set.h>
#include <util/generic/map.h>

namespace NKikimr::NCms::NSentinel {

using TLimitsMap = TMap<EPDiskState, ui32>;

class TPDiskStatusComputer {
public:
    explicit TPDiskStatusComputer(const ui32& defaultStateLimit, const ui32& goodStateLimit, const TLimitsMap& stateLimits);

    void AddState(EPDiskState state, bool isNodeLocked);
    EPDiskStatus Compute(EPDiskStatus current, TString& reason) const;

    EPDiskState GetState() const;
    EPDiskState GetPrevState() const;
    ui64 GetStateCounter() const;

    void Reset();

    void SetForcedStatus(EPDiskStatus status);
    bool HasForcedStatus() const;
    void ResetForcedStatus();

private:
    const ui32& DefaultStateLimit;
    const ui32& GoodStateLimit;
    const TLimitsMap& StateLimits;

    EPDiskState State = NKikimrBlobStorage::TPDiskState::Unknown;
    mutable EPDiskState PrevState = State;
    ui64 StateCounter;
    TMaybe<EPDiskStatus> ForcedStatus;

    mutable bool HadBadStateRecently = false;

}; // TPDiskStatusComputer

class TPDiskStatus: public TPDiskStatusComputer {
public:
    explicit TPDiskStatus(EPDiskStatus initialStatus, const ui32& defaultStateLimit, const ui32& goodStateLimit, const TLimitsMap& stateLimits);

    void AddState(EPDiskState state, bool isNodeLocked);
    bool IsChanged() const;
    void ApplyChanges(TString& reason);
    void ApplyChanges();
    EPDiskStatus GetStatus() const;
    bool IsNewStatusGood() const;

    bool IsChangingAllowed() const;
    void AllowChanging();
    void DisallowChanging();

private:
    EPDiskStatus Current;
    bool ChangingAllowed;

}; // TPDiskStatus

struct TStatusChangerState: public TSimpleRefCount<TStatusChangerState> {
    using TPtr = TIntrusivePtr<TStatusChangerState>;

    explicit TStatusChangerState(NKikimrBlobStorage::EDriveStatus status)
        : Status(status)
    {
    }

    const NKikimrBlobStorage::EDriveStatus Status;
    ui32 Attempt = 0;

}; // TStatusChangerState

struct TPDiskInfo
    : public TSimpleRefCount<TPDiskInfo>
    , public TPDiskStatus
{
    using TPtr = TIntrusivePtr<TPDiskInfo>;
    using EIgnoreReason = NKikimrCms::TPDiskInfo::EIgnoreReason;

    EPDiskStatus ActualStatus = EPDiskStatus::ACTIVE;
    EPDiskStatus PrevStatus = EPDiskStatus::UNKNOWN;
    TInstant LastStatusChange;
    bool StatusChangeFailed = false;
    // means that this pdisk status change last time was the reason of whole request failure
    bool LastStatusChangeFailed = false;
    ui32 StatusChangeAttempt = 0;
    ui32 PrevStatusChangeAttempt = 0;
    EIgnoreReason IgnoreReason = NKikimrCms::TPDiskInfo::NOT_IGNORED;

    explicit TPDiskInfo(EPDiskStatus initialStatus, const ui32& defaultStateLimit, const ui32& goodStateLimit, const TLimitsMap& stateLimits);

    bool IsTouched() const { return Touched; }
    void Touch() { Touched = true; }
    void ClearTouched() { Touched = false; }

    void AddState(EPDiskState state, bool isNodeLocked);

private:
    bool Touched;

}; // TPDiskInfo

struct TNodeStatusComputer {
    enum ENodeState {
        GOOD = 0,
        PRETTY_GOOD,
        MAY_BE_GOOD,
        MAY_BE_BAD,
        BAD,
    };

    ENodeState CurrentState = ENodeState::GOOD;
    ENodeState ActualState = ENodeState::GOOD;
    ui64 StateCounter = 0;
    ui32 BadStateLimit;
    ui32 GoodStateLimit;
    ui32 PrettyGoodStateLimit;

    ui32 GetCurrentNodeState() const;
    bool Compute();
    void AddState(ENodeState newState);

    bool MaybeBad() const { return CurrentState == ENodeState::BAD && StateCounter < BadStateLimit; }
    bool DefinitelyBad() const { return CurrentState == ENodeState::BAD && StateCounter >= BadStateLimit; }
    bool MaybeGood() const { return CurrentState == ENodeState::GOOD && StateCounter < PrettyGoodStateLimit; }
    bool PrettyGood() const { return CurrentState == ENodeState::GOOD && StateCounter >= PrettyGoodStateLimit; }
    bool DefinitelyGood() const { return CurrentState == ENodeState::GOOD && StateCounter >= GoodStateLimit; }
};

struct TNodeInfo : public TNodeStatusComputer {
    TString Host;
    NActors::TNodeLocation Location;
    TMaybeFail<ui32> PileId;
    THashSet<NKikimrCms::EMarker> Markers;

    bool HasFaultyMarker() const;
};

struct TConfigUpdaterState {
    ui32 BSCAttempt = 0;
    ui32 CMSAttempt = 0;
    bool GotBSCResponse = false;
    bool GotCMSResponse = false;

    void Clear() {
        *this = TConfigUpdaterState{};
    }
};

/// Main state
struct TSentinelState: public TSimpleRefCount<TSentinelState> {
    using TPtr = TIntrusivePtr<TSentinelState>;
    using TNodeId = ui32;

    TMap<TPDiskID, TPDiskInfo::TPtr> PDisks;
    TMap<TNodeId, TNodeInfo> Nodes;
    THashSet<ui32> StateUpdaterWaitNodes;
    TConfigUpdaterState ConfigUpdaterState;
    TConfigUpdaterState PrevConfigUpdaterState;
    TMap<TPDiskID, TPDiskInfo::TPtr> ChangeRequests;
    ui32 StatusChangeAttempt = 0;
    ui32 ChangeRequestId = 0;
    bool NeedSelfHealStateStorage = false;
    TInstant LastStateStorageSelfHeal = TInstant::Zero();
};

class TClusterMap {
public:
    using TPDiskIDSet = THashSet<TPDiskID, TPDiskIDHash>;
    using TPDiskIgnoredMap = THashMap<TPDiskID, TPDiskInfo::EIgnoreReason, TPDiskIDHash>;
    using TDistribution = THashMap<TString, TPDiskIDSet>;
    using TNodeIDSet = THashSet<ui32>;

    TSentinelState::TPtr State;
    TDistribution ByDataCenter;
    TDistribution ByRoom;
    TDistribution ByRack;
    TDistribution ByPile;
    THashMap<TString, TNodeIDSet> NodeByRack;
    TDistribution BadByNode;

    explicit TClusterMap(TSentinelState::TPtr state);

    void AddPDisk(const TPDiskID& id, const bool inGoodState = true);

}; // TClusterMap

class TGuardian : public TClusterMap {
    static bool CheckRatio(ui32 check, ui32 base, ui32 ratio) {
        return (check * 100) <= (base * ratio);
    }

    static bool CheckRatio(const TDistribution::value_type& check, const TDistribution& base, ui32 ratio) {
        return CheckRatio(check.second.size(), base.at(check.first).size(), ratio);
    }

public:
    explicit TGuardian(TSentinelState::TPtr state, ui32 dataCenterRatio = 100, ui32 roomRatio = 100,
                       ui32 rackRatio = 100, ui32 pileRatio = 100, ui32 faultyPDisksThresholdPerNode = 0);

    TPDiskIDSet GetAllowedPDisks(const TClusterMap& all, TString& issues, TPDiskIgnoredMap& disallowed) const;

private:
    const ui32 DataCenterRatio;
    const ui32 RoomRatio;
    const ui32 RackRatio;
    const ui32 PileRatio;
    const ui32 FaultyPDisksThresholdPerNode;

}; // TGuardian

} // namespace NKikimr::NCms::NSentinel
