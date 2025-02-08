#include "MoHSESListener.h"
#include <sstream>
#include "managers/NodeDataManager.hpp"
#include "managers/StatusManager.hpp"
#include "managers/LabsManager.hpp"

MoHSESListener::MoHSESListener() : m_lastTick(0) {}

void MoHSESListener::onNewStatus(AMM::Status& st, SampleInfo_t* info) {
	std::ostringstream statusValue;
	statusValue << AMM::Utility::EStatusValueStr(st.value());

	auto& statusManager = StatusManager::getInstance();

	if (st.module_name() == "AMM_FluidManager" && st.capability().empty()) {
		statusManager.set("FLUIDICS_STATE", statusValue.str());
	}

	if (st.module_name() == "AMM_FluidManager" && st.capability() == "clear_supply") {
		statusManager.set("CLEAR_SUPPLY", statusValue.str());
	}

	if (st.module_name() == "AMM_FluidManager" && st.capability() == "blood_supply") {
		statusManager.set("BLOOD_SUPPLY", statusValue.str());
	}

	if (st.capability() == "iv_detection") {
		statusManager.set("IVARM_STATE", statusValue.str());
	}
}

void MoHSESListener::onNewTick(AMM::Tick& t, SampleInfo_t* info) {
	auto& statusManager = StatusManager::getInstance();

	if (statusManager.get("STATUS") == "NOT RUNNING" && t.frame() > getLastTick()) {
		statusManager.set("STATUS", "RUNNING");
	}

	setLastTick(t.frame());
	statusManager.set("TICK", std::to_string(t.frame()));
	statusManager.set("TIME", std::to_string(t.time()));
}

void MoHSESListener::onNewCommand(AMM::Command& c, SampleInfo_t* info) {
	const std::string sysPrefix = "[SYS]";
	auto& statusManager = StatusManager::getInstance();
	auto& nodeManager = NodeDataManager::getInstance();
	auto& labsManager = LabsManager::getInstance();

	if (!c.message().compare(0, sysPrefix.size(), sysPrefix)) {
		std::string value = c.message().substr(sysPrefix.size());
		if (value == "START_SIM") {
			statusManager.set("STATUS", "RUNNING");
		} else if (value == "STOP_SIM") {
			statusManager.set("STATUS", "STOPPED");
		} else if (value == "PAUSE_SIM") {
			statusManager.set("STATUS", "PAUSED");
		} else if (value == "RESET_SIM") {
			statusManager.set("STATUS", "NOT RUNNING");
			statusManager.set("TICK", "0");
			statusManager.set("TIME", "0");
			nodeManager.clear();
			labsManager.clear();
		}
	}
}

void MoHSESListener::onNewPhysiologyValue(AMM::PhysiologyValue& n, SampleInfo_t* info) {
	NodeDataManager::getInstance().set(n.name(), n.value());
}

int64_t MoHSESListener::getLastTick() const {
	return m_lastTick.load(std::memory_order_acquire);
}

void MoHSESListener::setLastTick(int64_t tick) {
	m_lastTick.store(tick, std::memory_order_release);
}