#pragma once

#include <atomic>
#include <string>
#include "amm_std.h"

class MoHSESListener {
public:
	MoHSESListener();

	void onNewStatus(AMM::Status& st, SampleInfo_t* info);
	void onNewTick(AMM::Tick& t, SampleInfo_t* info);
	void onNewCommand(AMM::Command& c, SampleInfo_t* info);
	void onNewPhysiologyValue(AMM::PhysiologyValue& n, SampleInfo_t* info);

	int64_t getLastTick() const;
	void setLastTick(int64_t tick);

private:
	std::atomic<int64_t> m_lastTick;
};