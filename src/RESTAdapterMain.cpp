#include <mutex>
#include <chrono>
#include <thread>
#include <iostream>
#include <functional>
#include <algorithm>

#include <pistache/description.h>
#include <pistache/endpoint.h>
#include <pistache/http.h>
#include <pistache/router.h>

#include "amm_std.h"
#include "amm/Utility.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>

#include "thirdparty/sqlite_modern_cpp.h"
#include "DatabaseQueries.hpp"
#include "DatabaseConnection.hpp"

using namespace std;
using namespace std::chrono;
using namespace boost::filesystem;
using namespace rapidjson;

/// Hostname to connect to.
char hostname[HOST_NAME_MAX];

std::string action_path = "Actions/";
std::string state_path = "./states/";
std::string patient_path = "./patients/";
std::string scenario_path = "./static/scenarios/";

namespace {
	std::mutex nodeDataMutex;
	std::mutex statusMutex;
	std::mutex labsMutex;

	// Thread-safe wrapper for nodeDataStorage
	class NodeDataManager {
	private:
		std::map<std::string, double> storage;
		mutable std::shared_mutex mutex;  // allows multiple readers, single writer

	public:
		void clear() {
			std::unique_lock<std::shared_mutex> lock(mutex);
			storage.clear();
		}

		void set(const std::string &key, double value) {
			std::unique_lock<std::shared_mutex> lock(mutex);
			if (!std::isnan(value)) {
				storage[key] = value;
			}
		}

		std::optional<double> get(const std::string &key) const {
			std::shared_lock<std::shared_mutex> lock(mutex);
			auto it = storage.find(key);
			if (it != storage.end()) {
				return it->second;
			}
			return std::nullopt;
		}

		std::map<std::string, double> getAll() const {
			std::shared_lock<std::shared_mutex> lock(mutex);
			return storage;
		}
	};

	// Thread-safe wrapper for statusStorage
	class StatusManager {
	private:
		std::map<std::string, std::string> storage;
		mutable std::mutex mutex;

	public:
		StatusManager() {
			storage = {
					{"STATUS", "NOT RUNNING"},
					{"TICK", "0"},
					{"TIME", "0"},
					{"SCENARIO", ""},
					{"STATE", ""},
					{"CLEAR_SUPPLY", ""},
					{"BLOOD_SUPPLY", ""},
					{"FLUIDICS_STATE", ""},
					{"IVARM_STATE", ""}
			};
		}

		void set(const std::string &key, const std::string &value) {
			std::lock_guard<std::mutex> lock(mutex);
			storage[key] = value;
		}

		std::string get(const std::string &key) const {
			std::lock_guard<std::mutex> lock(mutex);
			auto it = storage.find(key);
			return it != storage.end() ? it->second : "";
		}

		std::map<std::string, std::string> getAll() const {
			std::lock_guard<std::mutex> lock(mutex);
			return storage;
		}
	};

	// Thread-safe wrapper for labsStorage
	class LabsManager {
	private:
		std::vector<std::string> storage;
		mutable std::mutex mutex;

	public:
		void clear() {
			std::lock_guard<std::mutex> lock(mutex);
			storage.clear();
		}

		void append(const std::string &row) {
			std::lock_guard<std::mutex> lock(mutex);
			storage.push_back(row);
		}

		std::vector<std::string> getAll() const {
			std::lock_guard<std::mutex> lock(mutex);
			return storage;
		}
	};
}

NodeDataManager nodeDataManager;
StatusManager statusManager;
LabsManager labsManager;

std::atomic<bool> m_runThread = true;
int64_t lastTick = 0;

const string sysPrefix = "[SYS]";
const string actPrefix = "[ACT]";
const string loadScenarioPrefix = "LOAD_SCENARIO:";
const string loadScenarioFilePrefix = "LOAD_SCENARIOFILE:";
const string loadPrefix = "LOAD_STATE:";
const string loadPatientPrefix = "LOAD_PATIENT:";


void ResetLabs() {
	try {
		std::ostringstream labRow;

		// Build header row
		labRow << "Time,";

		// POCT
		labRow << "POCT,";
		labRow << "Sodium (Na),";
		labRow << "Potassium (K),";
		labRow << "Chloride (Cl),";
		labRow << "TCO2,";
		labRow << "Anion Gap,";
		labRow << "Ionized Calcium (iCa),";
		labRow << "Glucose (Glu),";
		labRow << "Urea Nitrogen (BUN)/Urea,";
		labRow << "Creatinine (Crea),";

		// Hematology
		labRow << "Hematology,";
		labRow << "Hematocrit (Hct),";
		labRow << "Hemoglobin (Hgb),";

		// ABG
		labRow << "ABG,";
		labRow << "Lactate,";
		labRow << "pH,";
		labRow << "PCO2,";
		labRow << "PO2,";
		labRow << "TCO2,";
		labRow << "HCO3,";
		labRow << "Base Excess (BE),";
		labRow << "SpO2,";
		labRow << "COHb,";

		// VBG
		labRow << "VBG,";
		labRow << "Lactate,";
		labRow << "pH,";
		labRow << "PCO2,";
		labRow << "TCO2,";
		labRow << "HCO3,";
		labRow << "Base Excess (BE),";
		labRow << "COHb,";

		// BMP
		labRow << "BMP,";
		labRow << "Sodium (Na),";
		labRow << "Potassium (K),";
		labRow << "Chloride (Cl),";
		labRow << "TCO2,";
		labRow << "Anion Gap,";
		labRow << "Ionized Calcium (iCa),";
		labRow << "Glucose (Glu),";
		labRow << "Urea Nitrogen (BUN)/Urea,";
		labRow << "Creatinine (Crea),";

		// CBC
		labRow << "CBC,";
		labRow << "WBC,";
		labRow << "RBC,";
		labRow << "Hgb,";
		labRow << "Hct,";
		labRow << "Plt,";

		// CMP
		labRow << "CMP,";
		labRow << "Albumin,";
		labRow << "ALP,";
		labRow << "ALT,";
		labRow << "AST,";
		labRow << "BUN,";
		labRow << "Calcium,";
		labRow << "Chloride,";
		labRow << "CO2,";
		labRow << "Creatinine (men),";
		labRow << "Creatinine (women),";
		labRow << "Glucose,";
		labRow << "Potassium,";
		labRow << "Sodium,";
		labRow << "Total bilirubin,";
		labRow << "Total protein";

		// Clear existing data and add header
		labsManager.clear();
		labsManager.append(labRow.str());

		LOG_INFO << "Labs reset successfully";
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Error resetting labs: " << e.what();
		throw;
	}
}

void AppendLabRow() {
	try {
		std::ostringstream labRow;
		auto nodeData = nodeDataManager.getAll();  // Get thread-safe copy of all node data

		// Time
		labRow << nodeData["SIM_TIME"] << ",";

		// POCT
		labRow << "POCT,";
		labRow << nodeData["Substance_Sodium"] << ",";
		labRow << nodeData["MetabolicPanel_Potassium"] << ",";
		labRow << nodeData["MetabolicPanel_Chloride"] << ",";
		labRow << nodeData["MetabolicPanel_CarbonDioxide"] << ",";
		labRow << ",";  // Anion Gap
		labRow << ",";  // Ionized Calcium (iCa)
		labRow << nodeData["Substance_Glucose_Concentration"] << ",";
		labRow << nodeData["BloodChemistry_BloodUreaNitrogen_Concentration"] << ",";
		labRow << nodeData["Substance_Creatinine_Concentration"] << ",";

		// Hematology
		labRow << "Hematology,";
		labRow << nodeData["BloodChemistry_Hemaocrit"] << ",";
		labRow << nodeData["Substance_Hemoglobin_Concentration"] << ",";

		// ABG
		labRow << "ABG,";
		labRow << nodeData["Substance_Lactate_Concentration_mmol"] << ",";
		labRow << nodeData["BloodChemistry_BloodPH"] << ",";
		labRow << nodeData["BloodChemistry_Arterial_CarbonDioxide_Pressure"] << ",";
		labRow << nodeData["BloodChemistry_Arterial_Oxygen_Pressure"] << ",";
		labRow << nodeData["MetabolicPanel_CarbonDioxide"] << ",";
		labRow << nodeData["Substance_Bicarbonate"] << ",";
		labRow << nodeData["Substance_BaseExcess"] << ",";
		labRow << nodeData["BloodChemistry_Oxygen_Saturation"] << ",";
		labRow << nodeData["Substance_Carboxyhemoglobin_Concentration"] << ",";

		// VBG
		labRow << "VBG,";
		labRow << nodeData["Substance_Lactate_Concentration_mmol"] << ",";
		labRow << nodeData["BloodChemistry_BloodPH"] << ",";
		labRow << nodeData["BloodChemistry_VenousCarbonDioxidePressure"] << ",";
		labRow << nodeData["MetabolicPanel_CarbonDioxide"] << ",";
		labRow << nodeData["Substance_Bicarbonate"] << ",";
		labRow << nodeData["Substance_BaseExcess"] << ",";
		labRow << nodeData["Substance_Carboxyhemoglobin_Concentration"] << ",";

		// BMP
		labRow << "BMP,";
		labRow << nodeData["Substance_Sodium"] << ",";
		labRow << nodeData["MetabolicPanel_Potassium"] << ",";
		labRow << nodeData["MetabolicPanel_Chloride"] << ",";
		labRow << nodeData["MetabolicPanel_CarbonDioxide"] << ",";
		labRow << ",";  // Anion Gap
		labRow << ",";  // Ionized Calcium (iCa)
		labRow << nodeData["Substance_Glucose_Concentration"] << ",";
		labRow << nodeData["BloodChemistry_BloodUreaNitrogen_Concentration"] << ",";
		labRow << nodeData["Substance_Creatinine_Concentration"] << ",";

		// CBC
		labRow << "CBC,";
		labRow << nodeData["BloodChemistry_WhiteBloodCell_Count"] << ",";
		labRow << nodeData["BloodChemistry_RedBloodCell_Count"] << ",";
		labRow << nodeData["Substance_Hemoglobin_Concentration"] << ",";
		labRow << nodeData["BloodChemistry_Hemaocrit"] << ",";
		labRow << nodeData["CompleteBloodCount_Platelet"] << ",";

		// CMP
		labRow << "CMP,";
		labRow << nodeData["Substance_Albumin_Concentration"] << ",";
		labRow << ",";  // ALP
		labRow << ",";  // ALT
		labRow << ",";  // AST
		labRow << nodeData["BloodChemistry_BloodUreaNitrogen_Concentration"] << ",";
		labRow << nodeData["Substance_Calcium_Concentration"] << ",";
		labRow << nodeData["MetabolicPanel_Chloride"] << ",";
		labRow << nodeData["MetabolicPanel_CarbonDioxide"] << ",";
		labRow << nodeData["Substance_Creatinine_Concentration"] << ",";
		labRow << nodeData["Substance_Creatinine_Concentration"] << ",";
		labRow << nodeData["Substance_Glucose_Concentration"] << ",";
		labRow << nodeData["MetabolicPanel_Potassium"] << ",";
		labRow << nodeData["Substance_Sodium"] << ",";
		labRow << nodeData["MetabolicPanel_Bilirubin"] << ",";
		labRow << nodeData["MetabolicPanel_Protein"];

		// Append the new row
		labsManager.append(labRow.str());

		LOG_INFO << "Lab row appended successfully";
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Error appending lab row: " << e.what();
		throw;
	}
}

class AMMListener {
public:
	void onNewStatus(AMM::Status &st, SampleInfo_t *info) {
		std::ostringstream statusValue;
		statusValue << AMM::Utility::EStatusValueStr(st.value());

		if (st.module_name() == "AMM_FluidManager" && st.capability() == "") {
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

	void onNewTick(AMM::Tick &t, SampleInfo_t *info) {
		if (statusManager.get("STATUS") == "NOT RUNNING" && t.frame() > lastTick) {
			statusManager.set("STATUS", "RUNNING");
		}
		lastTick = t.frame();
		statusManager.set("TICK", std::to_string(t.frame()));
		statusManager.set("TIME", std::to_string(t.time()));
	}

	void onNewCommand(AMM::Command &c, SampleInfo_t *info) {
		if (!c.message().compare(0, sysPrefix.size(), sysPrefix)) {
			std::string value = c.message().substr(sysPrefix.size());
			if (value.compare("START_SIM") == 0) {
				statusManager.set("STATUS", "RUNNING");
			} else if (value.compare("STOP_SIM") == 0) {
				statusManager.set("STATUS", "STOPPED");
			} else if (value.compare("PAUSE_SIM") == 0) {
				statusManager.set("STATUS", "PAUSED");
			} else if (value.compare("RESET_SIM") == 0) {
				statusManager.set("STATUS", "NOT RUNNING");
				statusManager.set("TICK", "0");
				statusManager.set("TIME", "0");
				nodeDataManager.clear();
				ResetLabs();
			}
		}
	}

	void onNewPhysiologyValue(AMM::PhysiologyValue &n, SampleInfo_t *info) {
		nodeDataManager.set(n.name(), n.value());
	}
};

const std::string moduleName = "AMM_REST_Adapter";
const std::string configFile = "config/rest_adapter_amm.xml";
std::unique_ptr<AMM::DDSManager<AMMListener>> mgr = std::make_unique<AMM::DDSManager<AMMListener>>(configFile);
AMM::UUID m_uuid;

void SendReset() {
	AMM::SimulationControl simControl;
	auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	simControl.timestamp(ms);
	simControl.type(AMM::ControlType::RESET);
	mgr->WriteSimulationControl(simControl);
}

void PublishOperationalDescription() {
	AMM::OperationalDescription od;
	od.name(moduleName);
	od.model("REST Adapter");
	od.manufacturer("Vcom3D");
	od.serial_number("1.0.0");
	od.module_id(m_uuid);
	od.module_version("1.0.0");
	const std::string capabilities = AMM::Utility::read_file_to_string("config/rest_adapter_capabilities.xml");
	od.capabilities_schema(capabilities);
	od.description();
	mgr->WriteOperationalDescription(od);
}

void PublishConfiguration() {
	AMM::ModuleConfiguration mc;
	auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	mc.timestamp(ms);
	mc.module_id(m_uuid);
	mc.name(moduleName);
	const std::string configuration = AMM::Utility::read_file_to_string("config/rest_adapter_configuration.xml");
	mc.capabilities_configuration(configuration);
	mgr->WriteModuleConfiguration(mc);
}

AMM::UUID SendEventRecord(
		const std::string &location,
		const std::string &practitioner,
		const std::string &type
) {
	AMM::EventRecord er;
	AMM::FMA_Location fmaL;
	fmaL.name(location);
	er.type(type);
	er.location(fmaL);
	AMM::UUID eventUUID;
	eventUUID.id(mgr->GenerateUuidString());
	mgr->WriteEventRecord(er);
	return eventUUID;
}

void SendPhysiologyModification(
		AMM::UUID &er_id,
		const std::string &type,
		const std::string &payload) {
	AMM::PhysiologyModification modInstance;
	AMM::UUID instUUID;
	instUUID.id(mgr->GenerateUuidString());
	modInstance.id(instUUID);
	modInstance.type(type);
	modInstance.event_id(er_id);
	modInstance.data(payload);
	mgr->WritePhysiologyModification(modInstance);
}

void SendRenderModification(
		AMM::UUID &er_id,
		const std::string &type,
		const std::string &payload) {
	AMM::RenderModification modInstance;
	AMM::UUID instUUID;
	instUUID.id(mgr->GenerateUuidString());
	modInstance.id(instUUID);
	modInstance.type(type);
	modInstance.event_id(er_id);
	modInstance.data(payload);
	mgr->WriteRenderModification(modInstance);
}

void SendPerformanceAssessment(
		AMM::UUID &er_id,
		const std::string &assessment_type,
		const std::string &assessment_info,
		const std::string &step,
		const std::string &comment) {
	AMM::Assessment assessInstance;
	AMM::UUID instUUID;
	instUUID.id(mgr->GenerateUuidString());
	assessInstance.id(instUUID);
	assessInstance.event_id(er_id);
	assessInstance.comment(comment);
	mgr->WriteAssessment(assessInstance);
}

void SendCommand(const std::string &command) {
	try {
		if (!command.compare(0, sysPrefix.size(), sysPrefix)) {
			std::string value = command.substr(sysPrefix.size());

			if (value.compare("START_SIM") == 0) {
				AMM::SimulationControl simControl;
				auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
				simControl.timestamp(ms);
				simControl.type(AMM::ControlType::RUN);
				mgr->WriteSimulationControl(simControl);
				statusManager.set("STATUS", "RUNNING");
			} else if (value.compare("STOP_SIM") == 0) {
				AMM::SimulationControl simControl;
				auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
				simControl.timestamp(ms);
				simControl.type(AMM::ControlType::HALT);
				mgr->WriteSimulationControl(simControl);
				statusManager.set("STATUS", "STOPPED");
			} else if (value.compare("PAUSE_SIM") == 0) {
				AMM::SimulationControl simControl;
				auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
				simControl.timestamp(ms);
				simControl.type(AMM::ControlType::HALT);
				mgr->WriteSimulationControl(simControl);
				statusManager.set("STATUS", "PAUSED");
			} else if (value.compare("RESET_SIM") == 0) {
				statusManager.set("STATUS", "NOT RUNNING");
				statusManager.set("TICK", "0");
				statusManager.set("TIME", "0");
				nodeDataManager.clear();
				ResetLabs();

				AMM::SimulationControl simControl;
				auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
				simControl.timestamp(ms);
				simControl.type(AMM::ControlType::RESET);
				mgr->WriteSimulationControl(simControl);
			} else if (value.compare("SAVE_STATE") == 0) {
				AMM::SimulationControl simControl;
				auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
				simControl.timestamp(ms);
				simControl.type(AMM::ControlType::SAVE);
				mgr->WriteSimulationControl(simControl);
			} else {
				// Publish a SYS Command
				AMM::Command cmdInstance;
				cmdInstance.message(command);
				mgr->WriteCommand(cmdInstance);
			}
		} else {
			// Publish regular command
			AMM::Command cmdInstance;
			cmdInstance.message(command);
			mgr->WriteCommand(cmdInstance);
		}
	}
	catch (const std::exception &e) {
		LOG_ERROR << "Error executing command '" << command << "': " << e.what();
		throw;
	}
}


class DDSEndpoint {
public:
	explicit DDSEndpoint(Pistache::Address addr)
			: httpEndpoint(std::make_shared<Pistache::Http::Endpoint>(addr)) {}

	static constexpr size_t DefaultMaxPayload = 1024102410;
	mutable std::mutex m_mutex;
	std::atomic<bool> m_isRunning{true};

	typedef std::mutex Lock;
	typedef std::lock_guard<Lock> Guard;
	Lock commandLock;

	std::shared_ptr<Pistache::Http::Endpoint> httpEndpoint;
	Pistache::Rest::Router router;
	std::unique_ptr<DatabaseConnection> m_db;

	void init(int thr = 2) {
		auto opts = Pistache::Http::Endpoint::options()
				.threads(static_cast<int>(thr));
		httpEndpoint->init(opts);
		setupRoutes();
	}

	void start() {
		httpEndpoint->setHandler(router.handler());
		httpEndpoint->serve();
	}

	void shutdown() {
		httpEndpoint->shutdown();
	}

private:
	void handleReady(const Pistache::Rest::Request&,
	                 Pistache::Http::ResponseWriter response) {
		response.send(Pistache::Http::Code::Ok, "1");
	}

	void setupRoutes() {

		Pistache::Rest::Routes::Get(router, "/ready", Pistache::Rest::Routes::bind(&DDSEndpoint::handleReady, this));
		Pistache::Rest::Routes::Get(router, "/instance", Pistache::Rest::Routes::bind(&DDSEndpoint::getInstance, this));
		Pistache::Rest::Routes::Get(router, "/node/:name", Pistache::Rest::Routes::bind(&DDSEndpoint::getNode, this));
		Pistache::Rest::Routes::Get(router, "/nodes", Pistache::Rest::Routes::bind(&DDSEndpoint::getNodes, this));
		Pistache::Rest::Routes::Get(router, "/command/:name", Pistache::Rest::Routes::bind(&DDSEndpoint::issueCommand, this));
		Pistache::Rest::Routes::Get(router, "/debug", Pistache::Rest::Routes::bind(&DDSEndpoint::doDebug, this));
		Pistache::Rest::Routes::Get(router, "/labs", Pistache::Rest::Routes::bind(&DDSEndpoint::getLabsReport, this));
		Pistache::Rest::Routes::Get(router, "/events", Pistache::Rest::Routes::bind(&DDSEndpoint::getEventLog, this));
		Pistache::Rest::Routes::Get(router, "/events/csv", Pistache::Rest::Routes::bind(&DDSEndpoint::getEventLogCSV, this));
		Pistache::Rest::Routes::Get(router, "/logs", Pistache::Rest::Routes::bind(&DDSEndpoint::getDiagnosticLog, this));
		Pistache::Rest::Routes::Get(router, "/logs/csv", Pistache::Rest::Routes::bind(&DDSEndpoint::getDiagnosticLogCSV, this));
		Pistache::Rest::Routes::Get(router, "/modules/count", Pistache::Rest::Routes::bind(&DDSEndpoint::getModuleCount, this));
		Pistache::Rest::Routes::Get(router, "/modules", Pistache::Rest::Routes::bind(&DDSEndpoint::getModules, this));
		Pistache::Rest::Routes::Get(router, "/module/id/:id", Pistache::Rest::Routes::bind(&DDSEndpoint::getModuleById, this));
		Pistache::Rest::Routes::Get(router, "/module/guid/:guid", Pistache::Rest::Routes::bind(&DDSEndpoint::getModuleByGuid, this));
		Pistache::Rest::Routes::Get(router, "/modules/other", Pistache::Rest::Routes::bind(&DDSEndpoint::getOtherModules, this));
		Pistache::Rest::Routes::Get(router, "/shutdown", Pistache::Rest::Routes::bind(&DDSEndpoint::doShutdown, this));
		Pistache::Rest::Routes::Get(router, "/actions", Pistache::Rest::Routes::bind(&DDSEndpoint::getActions, this));
		Pistache::Rest::Routes::Get(router, "/action/:name", Pistache::Rest::Routes::bind(&DDSEndpoint::getAction, this));
		Pistache::Rest::Routes::Post(router, "/action", Pistache::Rest::Routes::bind(&DDSEndpoint::createAction, this));
		Pistache::Rest::Routes::Put(router, "/action/:name", Pistache::Rest::Routes::bind(&DDSEndpoint::updateAction, this));
		Pistache::Rest::Routes::Delete(router, "/action/:name", Pistache::Rest::Routes::bind(&DDSEndpoint::deleteAction, this));
		Pistache::Rest::Routes::Get(router, "/assessments", Pistache::Rest::Routes::bind(&DDSEndpoint::getAssessments, this));
		Pistache::Rest::Routes::Get(router, "/assessment/:name", Pistache::Rest::Routes::bind(&DDSEndpoint::getAssessment, this));
		Pistache::Rest::Routes::Post(router, "/assessment/:name", Pistache::Rest::Routes::bind(&DDSEndpoint::createAssessment, this));
		Pistache::Rest::Routes::Put(router, "/assessment/:name", Pistache::Rest::Routes::bind(&DDSEndpoint::createAssessment, this));
		Pistache::Rest::Routes::Delete(router, "/assessment/:name", Pistache::Rest::Routes::bind(&DDSEndpoint::deleteAssessment, this));
		Pistache::Rest::Routes::Post(router, "/execute", Pistache::Rest::Routes::bind(&DDSEndpoint::executeCommand, this));
		Pistache::Rest::Routes::Post(router, "/topic/physiology_modification",
		     Pistache::Rest::Routes::bind(&DDSEndpoint::executePhysiologyModification, this));
		Pistache::Rest::Routes::Post(router, "/topic/render_modification",
		     Pistache::Rest::Routes::bind(&DDSEndpoint::executeRenderModification, this));
		Pistache::Rest::Routes::Post(router, "/topic/performance_assessment",
		     Pistache::Rest::Routes::bind(&DDSEndpoint::executePerformanceAssessment, this));
		Pistache::Rest::Routes::Options(router, "/topic/:mod_type", Pistache::Rest::Routes::bind(&DDSEndpoint::executeOptions, this));
		Pistache::Rest::Routes::Get(router, "/patients", Pistache::Rest::Routes::bind(&DDSEndpoint::getPatients, this));
		Pistache::Rest::Routes::Get(router, "/scenarios", Pistache::Rest::Routes::bind(&DDSEndpoint::getScenarios, this));
		Pistache::Rest::Routes::Get(router, "/states", Pistache::Rest::Routes::bind(&DDSEndpoint::getStates, this));
		Pistache::Rest::Routes::Get(router, "/states/:name/delete", Pistache::Rest::Routes::bind(&DDSEndpoint::deleteState, this));
	}


	void getInstance(const Pistache::Rest::Request&,
	                 Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);

			std::ifstream file("static/current_scenario.txt");
			if (!file) {
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Error: Unable to open scenario file.");
				return;
			}

			std::string scenario((std::istreambuf_iterator<char>(file)),
			                     std::istreambuf_iterator<char>());
			file.close();

			writer.StartObject();
			writer.Key("name");
			writer.String(hostname);
			writer.Key("scenario");
			writer.String(scenario.c_str());
			writer.EndObject();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getInstance: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	// Node and Status Handlers
	void getNode(const Pistache::Rest::Request& request,
	             Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();

		try {
			auto value = nodeDataManager.get(name);

			if (value.has_value()) {
				StringBuffer s;
				Writer<StringBuffer> writer(s);

				writer.StartObject();
				writer.Key(name.c_str());
				writer.Double(value.value());
				writer.EndObject();

				response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
				response.send(Pistache::Http::Code::Ok, s.GetString(),
				              MIME(Application, Json));
			} else {
				response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
				response.send(Pistache::Http::Code::Not_Found, "Node data does not exist");
			}
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getNode: " << e.what();
			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error retrieving node data: " + std::string(e.what()));
		}
	}

	void getNodes(const Pistache::Rest::Request&,
	              Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartArray();

			// Get thread-safe copies of data
			auto nodeData = nodeDataManager.getAll();
			auto statusData = statusManager.getAll();

			for (const auto& [key, value] : nodeData) {
				writer.StartObject();
				writer.Key(key.c_str());
				writer.Double(value);
				writer.EndObject();
			}

			for (const auto& [key, value] : statusData) {
				writer.StartObject();
				writer.Key(key.c_str());
				writer.String(value.c_str());
				writer.EndObject();
			}

			writer.EndArray();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getNodes: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error processing data: " + std::string(e.what()));
		}
	}

// Command and Debug Handlers
	void issueCommand(const Pistache::Rest::Request& request,
	                  Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();
		try {
			SendCommand(name);

			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartObject();
			writer.Key("message");
			writer.String(("Sent command: " + name).c_str());
			writer.EndObject();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString());
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in issueCommand: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error executing command: " + std::string(e.what()));
		}
	}

	void doDebug(const Pistache::Rest::Request&,
	             Pistache::Http::ResponseWriter response) {
		try {
			response.cookies().add(Pistache::Http::Cookie("lang", "en-US"));
			response.send(Pistache::Http::Code::Ok, "Debug mode enabled");
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in doDebug: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error enabling debug mode: " + std::string(e.what()));
		}
	}

	void getLabsReport(const Pistache::Rest::Request&,
	                   Pistache::Http::ResponseWriter response) {
		try {
			auto labs = labsManager.getAll();
			std::string labReport = boost::algorithm::join(labs, "\n");

			auto mime = Pistache::Http::Mime::MediaType::fromString("text/csv");
			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, labReport, mime);
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getLabsReport: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error generating lab report: " + std::string(e.what()));
		}
	}

	// Action Handlers
	void getActions(const Pistache::Rest::Request&,
	                Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartArray();

			if (exists(action_path) && is_directory(action_path)) {
				path p(action_path);
				if (is_directory(p)) {
					directory_iterator end_iter;
					for (directory_iterator dir_itr(p); dir_itr != end_iter; ++dir_itr) {
						if (is_regular_file(dir_itr->status())) {
							writer.StartObject();
							writer.Key("name");
							writer.String(dir_itr->path().filename().c_str());
							writer.Key("description");
							stringstream writeTime;
							writeTime << last_write_time(dir_itr->path());
							writer.String(writeTime.str().c_str());
							writer.EndObject();
						}
					}
				}
			}

			writer.EndArray();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getActions: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error retrieving actions: " + std::string(e.what()));
		}
	}

	void getAction(const Pistache::Rest::Request& request,
	               Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();
		try {
			std::string actionPath = action_path + name;
			if (!exists(actionPath)) {
				response.send(Pistache::Http::Code::Not_Found, "Action not found");
				return;
			}

			std::ifstream file(actionPath);
			if (!file) {
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Error reading action file");
				return;
			}

			std::string content((std::istreambuf_iterator<char>(file)),
			                    std::istreambuf_iterator<char>());

			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartObject();
			writer.Key("name");
			writer.String(name.c_str());
			writer.Key("content");
			writer.String(content.c_str());
			writer.EndObject();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getAction: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error retrieving action: " + std::string(e.what()));
		}
	}

	void createAction(const Pistache::Rest::Request& request,
	                  Pistache::Http::ResponseWriter response) {
		try {
			Document document;
			document.Parse(request.body().c_str());

			if (document.HasParseError()) {
				response.send(Pistache::Http::Code::Bad_Request, "Invalid JSON format");
				return;
			}

			if (!document.HasMember("name") || !document.HasMember("content")) {
				response.send(Pistache::Http::Code::Bad_Request,
				              "Missing required fields: name and content");
				return;
			}

			std::string name = document["name"].GetString();
			std::string content = document["content"].GetString();
			std::string actionPath = action_path + name;

			if (exists(actionPath)) {
				response.send(Pistache::Http::Code::Conflict, "Action already exists");
				return;
			}

			std::ofstream file(actionPath);
			if (!file) {
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Error creating action file");
				return;
			}

			file << content;
			file.close();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Created, "Action created successfully");
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in createAction: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error creating action: " + std::string(e.what()));
		}
	}

	void updateAction(const Pistache::Rest::Request& request,
	                  Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();
		try {
			Document document;
			document.Parse(request.body().c_str());

			if (document.HasParseError()) {
				response.send(Pistache::Http::Code::Bad_Request, "Invalid JSON format");
				return;
			}

			if (!document.HasMember("content")) {
				response.send(Pistache::Http::Code::Bad_Request,
				              "Missing required field: content");
				return;
			}

			std::string content = document["content"].GetString();
			std::string actionPath = action_path + name;

			if (!exists(actionPath)) {
				response.send(Pistache::Http::Code::Not_Found, "Action not found");
				return;
			}

			std::ofstream file(actionPath, std::ios::trunc);
			if (!file) {
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Error updating action file");
				return;
			}

			file << content;
			file.close();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, "Action updated successfully");
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in updateAction: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error updating action: " + std::string(e.what()));
		}
	}

	void deleteAction(const Pistache::Rest::Request& request,
	                  Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();
		try {
			std::string actionPath = action_path + name;

			if (!exists(actionPath)) {
				response.send(Pistache::Http::Code::Not_Found, "Action not found");
				return;
			}

			if (remove(actionPath.c_str()) != 0) {
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Error deleting action file");
				return;
			}

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, "Action deleted successfully");
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in deleteAction: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error deleting action: " + std::string(e.what()));
		}
	}

	// Assessment Handlers
	void getAssessments(const Pistache::Rest::Request&,
	                    Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartArray();

			const std::string assessmentPath = "assessments/";
			if (exists(assessmentPath) && is_directory(assessmentPath)) {
				path p(assessmentPath);
				directory_iterator end_iter;
				for (directory_iterator dir_itr(p); dir_itr != end_iter; ++dir_itr) {
					if (is_regular_file(dir_itr->status())) {
						writer.StartObject();
						writer.Key("name");
						writer.String(dir_itr->path().filename().c_str());
						writer.Key("last_modified");
						stringstream writeTime;
						writeTime << last_write_time(dir_itr->path());
						writer.String(writeTime.str().c_str());

						// Get file size
						writer.Key("size");
						writer.Int64(file_size(dir_itr->path()));
						writer.EndObject();
					}
				}
			}

			writer.EndArray();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getAssessments: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error retrieving assessments: " + std::string(e.what()));
		}
	}

	void getAssessment(const Pistache::Rest::Request& request,
	                   Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();
		try {
			std::string fileName = "assessments/" + name;

			if (!exists(fileName)) {
				response.send(Pistache::Http::Code::Not_Found, "Assessment not found");
				return;
			}

			// Serve the file directly
			Pistache::Http::serveFile(response, fileName.c_str());
			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getAssessment: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error retrieving assessment: " + std::string(e.what()));
		}
	}

	void createAssessment(const Pistache::Rest::Request& request,
	                      Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();
		if (name.empty()) {
			name = "test.csv";
		}

		try {
			std::string filename = "assessments/" + name;

			// Ensure the assessments directory exists
			create_directories("assessments");

			// Write the assessment data
			std::ofstream file(filename, std::ios::out | std::ios::trunc);
			if (!file) {
				LOG_ERROR << "Error opening file: " << filename;
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Error creating assessment file");
				return;
			}

			file << request.body();
			if (!file) {
				LOG_ERROR << "Error writing to file: " << filename;
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Error writing assessment data");
				return;
			}
			file.close();

			// Notify system about new assessment
			std::string command = "[SYS]ASSESSMENT_AVAILABLE:" + name;
			AMM::Command cmdInstance;
			cmdInstance.message(command);
			mgr->WriteCommand(cmdInstance);

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Created, "Assessment created successfully");

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in createAssessment: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error creating assessment: " + std::string(e.what()));
		}
	}

	void deleteAssessment(const Pistache::Rest::Request& request,
	                      Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();
		try {
			std::string fileName = "assessments/" + name;

			if (!exists(fileName)) {
				response.send(Pistache::Http::Code::Not_Found, "Assessment not found");
				return;
			}

			if (remove(fileName.c_str()) != 0) {
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Error deleting assessment file");
				return;
			}

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, "Assessment deleted successfully");

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in deleteAssessment: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error deleting assessment: " + std::string(e.what()));
		}
	}



	void getModules(const Pistache::Rest::Request&,
	                Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartArray();

			m_db->db() << Queries::GET_MODULES
			           >> [&writer](std::string module_id,
			                        std::string module_name,
			                        std::string description,
			                        std::string capabilities,
			                        std::string manufacturer,
			                        std::string model) {
				           writer.StartObject();
				           writer.Key("Module_ID");
				           writer.String(module_id.c_str());
				           writer.Key("Module_Name");
				           writer.String(module_name.c_str());
				           writer.Key("Description");
				           writer.String(description.c_str());
				           writer.Key("Manufacturer");
				           writer.String(manufacturer.c_str());
				           writer.Key("Model");
				           writer.String(model.c_str());
				           writer.Key("Module_Capabilities");
				           writer.String(capabilities.c_str());
				           writer.EndObject();
			           };

			writer.EndArray();
			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getModules: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}



	void getModuleById(const Pistache::Rest::Request& request,
	                   Pistache::Http::ResponseWriter response) {
		auto id = request.param(":id").as<std::string>();

		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			bool found = false;

			m_db->db() << Queries::GET_MODULE_BY_ID
			           >> [&](std::string module_id,
			                  std::string module_name,
			                  std::string description,
			                  std::string capabilities,
			                  std::string manufacturer,
			                  std::string model) {
				           found = true;
				           writer.StartObject();
				           writer.Key("Module_ID");
				           writer.String(module_id.c_str());
				           writer.Key("Module_Name");
				           writer.String(module_name.c_str());
				           writer.Key("Description");
				           writer.String(description.c_str());
				           writer.Key("Manufacturer");
				           writer.String(manufacturer.c_str());
				           writer.Key("Model");
				           writer.String(model.c_str());
				           writer.Key("Module_Capabilities");
				           writer.String(capabilities.c_str());
				           writer.EndObject();
			           }, id;

			if (!found) {
				response.send(Pistache::Http::Code::Not_Found, "Module not found");
				return;
			}

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getModuleById: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	void getModuleByGuid(const Pistache::Rest::Request& request,
	                     Pistache::Http::ResponseWriter response) {
		auto guid = request.param(":guid").as<std::string>();

		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			bool found = false;

			m_db->db() << Queries::GET_MODULE_BY_GUID
			           >> [&](std::string module_id,
			                  std::string module_guid,
			                  std::string module_name,
			                  std::string capabilities,
			                  std::string manufacturer,
			                  std::string model) {
				           found = true;
				           writer.StartObject();
				           writer.Key("Module_ID");
				           writer.String(module_id.c_str());
				           writer.Key("Module_GUID");
				           writer.String(module_guid.c_str());
				           writer.Key("Module_Name");
				           writer.String(module_name.c_str());
				           writer.Key("Manufacturer");
				           writer.String(manufacturer.c_str());
				           writer.Key("Model");
				           writer.String(model.c_str());
				           writer.Key("Module_Capabilities");
				           writer.String(capabilities.c_str());
				           writer.EndObject();
			           }, guid;

			if (!found) {
				response.send(Pistache::Http::Code::Not_Found, "Module not found");
				return;
			}

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getModuleByGuid: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	void getModuleCount(const Pistache::Rest::Request&,
	                    Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);

			m_db->db() << Queries::GET_MODULE_COUNT
			           >> [&writer](int total, int core) {
				           writer.StartObject();
				           writer.Key("module_count");
				           writer.Int(total);
				           writer.Key("core_count");
				           writer.Int(core);
				           writer.Key("other_count");
				           writer.Int(total - core);
				           writer.EndObject();
			           };

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getModuleCount: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	void getOtherModules(const Pistache::Rest::Request&,
	                     Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartArray();

			m_db->db() << Queries::GET_OTHER_MODULES
			           >> [&writer](std::string module_name) {
				           writer.String(module_name.c_str());
			           };

			writer.EndArray();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getOtherModules: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	void getEventLog(const Pistache::Rest::Request&,
	                 Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartArray();

			m_db->db() << Queries::GET_EVENT_LOG
			           >> [&writer](std::string module_id,
			                        std::string module_name,
			                        std::string source,
			                        std::string topic,
			                        int64_t timestamp,
			                        std::string data) {
				           writer.StartObject();
				           writer.Key("module_id");
				           writer.String(module_id.c_str());
				           writer.Key("source");
				           writer.String(module_name.c_str());
				           writer.Key("module_guid");
				           writer.String(source.c_str());
				           writer.Key("timestamp");
				           writer.Int64(timestamp);
				           writer.Key("topic");
				           writer.String(topic.c_str());
				           writer.Key("message");
				           writer.String(data.c_str());
				           writer.EndObject();
			           };

			writer.EndArray();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getEventLog: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}


	void getEventLogCSV(const Pistache::Rest::Request&,
	                    Pistache::Http::ResponseWriter response) {
		try {
			std::ostringstream csv;
			csv << "Timestamp,Module,Source,Topic,Data\n";

			m_db->db() << Queries::GET_EVENT_LOG
			           >> [&csv](std::string module_id,
			                     std::string module_name,
			                     std::string source,
			                     std::string topic,
			                     int64_t timestamp,
			                     std::string data) {
				           std::time_t temp = timestamp;
				           std::tm* t = std::gmtime(&temp);

				           auto escapeCommas = [](const std::string& str) {
					           if (str.find(',') != std::string::npos) {
						           return "\"" + str + "\"";
					           }
					           return str;
				           };

				           csv << std::put_time(t, "%Y-%m-%d %I:%M:%S %p") << ","
				               << escapeCommas(module_name) << ","
				               << escapeCommas(source) << ","
				               << escapeCommas(topic) << ","
				               << escapeCommas(data) << "\n";
			           };

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			auto csvHeader = Pistache::Http::Header::Raw("Content-Disposition",
			                                             "attachment;filename=amm_timeline_log.csv");
			response.headers().addRaw(csvHeader);
			response.send(Pistache::Http::Code::Ok, csv.str(),
			              Pistache::Http::Mime::MediaType::fromString("text/csv"));

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getEventLogCSV: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	void getDiagnosticLog(const Pistache::Rest::Request&,
	                      Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartArray();

			m_db->db() << Queries::GET_DIAGNOSTIC_LOG
			           >> [&writer](std::string module_name,
			                        std::string module_guid,
			                        std::string module_id,
			                        std::string message,
			                        std::string log_level,
			                        int64_t timestamp) {
				           writer.StartObject();
				           writer.Key("source");
				           writer.String(module_name.c_str());
				           writer.Key("module_guid");
				           writer.String(module_guid.c_str());
				           writer.Key("module_id");
				           writer.String(module_id.c_str());
				           writer.Key("timestamp");
				           writer.Int64(timestamp);
				           writer.Key("log_level");
				           writer.String(log_level.c_str());
				           writer.Key("message");
				           writer.String(message.c_str());
				           writer.EndObject();
			           };

			writer.EndArray();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getDiagnosticLog: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	void getDiagnosticLogCSV(const Pistache::Rest::Request&,
	                         Pistache::Http::ResponseWriter response) {
		try {
			std::ostringstream csv;
			csv << "Timestamp,LogLevel,Module,Message\n";

			m_db->db() << Queries::GET_DIAGNOSTIC_LOG
			           >> [&csv](std::string module_name,
			                     std::string module_guid,
			                     std::string module_id,
			                     std::string message,
			                     std::string log_level,
			                     int64_t timestamp) {
				           std::time_t temp = timestamp;
				           std::tm* t = std::gmtime(&temp);

				           auto escapeCommas = [](const std::string& str) {
					           if (str.find(',') != std::string::npos) {
						           return "\"" + str + "\"";
					           }
					           return str;
				           };

				           csv << std::put_time(t, "%Y-%m-%d %I:%M:%S %p") << ","
				               << escapeCommas(log_level) << ","
				               << escapeCommas(module_name) << ","
				               << escapeCommas(message) << "\n";
			           };

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			auto csvHeader = Pistache::Http::Header::Raw("Content-Disposition",
			                                             "attachment;filename=amm_diagnostic_log.csv");
			response.headers().addRaw(csvHeader);
			response.send(Pistache::Http::Code::Ok, csv.str(),
			              Pistache::Http::Mime::MediaType::fromString("text/csv"));

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getDiagnosticLogCSV: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	void executeCommand(const Pistache::Rest::Request& request,
	                    Pistache::Http::ResponseWriter response) {
		try {
			Document document;
			document.Parse(request.body().c_str());

			if (document.HasParseError()) {
				response.send(Pistache::Http::Code::Bad_Request, "Invalid JSON format");
				return;
			}

			if (!document.HasMember("payload") || !document["payload"].IsString()) {
				response.send(Pistache::Http::Code::Bad_Request,
				              "Missing or invalid 'payload' field");
				return;
			}

			std::string payload = document["payload"].GetString();

			try {
				SendCommand(payload);
			} catch (const std::exception& e) {
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Execution error: " + std::string(e.what()));
				return;
			}

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.headers().add<Pistache::Http::Header::AccessControlAllowHeaders>("*");
			response.send(Pistache::Http::Code::Ok, "{\"message\":\"Command executed\"}");
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in executeCommand: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	// Patient and Scenario Handlers
	void getPatients(const Pistache::Rest::Request&,
	                 Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartArray();

			if (exists(patient_path) && is_directory(patient_path)) {
				path p(patient_path);
				if (is_directory(p)) {
					directory_iterator end_iter;
					for (directory_iterator dir_itr(p); dir_itr != end_iter; ++dir_itr) {
						if (is_regular_file(dir_itr->status())) {
							writer.StartObject();
							writer.Key("name");
							writer.String(dir_itr->path().filename().c_str());
							writer.Key("last_modified");
							stringstream writeTime;
							writeTime << last_write_time(dir_itr->path());
							writer.String(writeTime.str().c_str());
							writer.EndObject();
						}
					}
				}
			}

			writer.EndArray();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getPatients: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error retrieving patients: " + std::string(e.what()));
		}
	}

	void getScenarios(const Pistache::Rest::Request&,
	                  Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartArray();

			if (exists(scenario_path) && is_directory(scenario_path)) {
				path p(scenario_path);
				if (is_directory(p)) {
					directory_iterator end_iter;
					for (directory_iterator dir_itr(p); dir_itr != end_iter; ++dir_itr) {
						if (is_regular_file(dir_itr->status())) {
							writer.StartObject();
							writer.Key("name");
							writer.String(dir_itr->path().filename().c_str());
							writer.Key("last_modified");
							stringstream writeTime;
							writeTime << last_write_time(dir_itr->path());
							writer.String(writeTime.str().c_str());
							writer.EndObject();
						}
					}
				}
			} else {
				LOG_ERROR << "Scenario path does not exist";
			}

			writer.EndArray();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getScenarios: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error retrieving scenarios: " + std::string(e.what()));
		}
	}

// State Handlers
	void getStates(const Pistache::Rest::Request&,
	               Pistache::Http::ResponseWriter response) {
		try {
			StringBuffer s;
			Writer<StringBuffer> writer(s);
			writer.StartArray();

			if (exists(state_path) && is_directory(state_path)) {
				path p(state_path);
				if (is_directory(p)) {
					std::vector<boost::filesystem::path> paths(
							boost::filesystem::directory_iterator{state_path},
							boost::filesystem::directory_iterator{}
					);
					std::sort(paths.begin(), paths.end());
					for (const auto& path : paths) {
						writer.StartObject();
						writer.Key("name");
						writer.String(path.filename().c_str());
						writer.Key("last_updated");
						stringstream writeTime;
						writeTime << last_write_time(path);
						writer.String(writeTime.str().c_str());
						writer.EndObject();
					}
				}
			}

			writer.EndArray();

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.send(Pistache::Http::Code::Ok, s.GetString(),
			              MIME(Application, Json));
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in getStates: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error retrieving states: " + std::string(e.what()));
		}
	}

	void deleteState(const Pistache::Rest::Request& request,
	                 Pistache::Http::ResponseWriter response) {
		auto name = request.param(":name").as<std::string>();
		response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");

		try {
			if (name == "StandardMale@0s.xml") {
				response.send(Pistache::Http::Code::Forbidden,
				              "Cannot delete default state file");
				return;
			}

			std::ostringstream deleteFile;
			deleteFile << state_path << "/" << name;
			path deletePath(deleteFile.str().c_str());

			if (exists(deletePath) && is_regular_file(deletePath)) {
				boost::filesystem::remove(deletePath);
				response.send(Pistache::Http::Code::Ok, "State deleted successfully");
			} else {
				response.send(Pistache::Http::Code::Not_Found,
				              "State file not found");
			}
		} catch (const std::exception& e) {
			LOG_ERROR << "Error in deleteState: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error deleting state: " + std::string(e.what()));
		}
	}

// CORS and Options Handlers
	void executeOptions(const Pistache::Rest::Request&,
	                    Pistache::Http::ResponseWriter response) {
		response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
		response.headers().add<Pistache::Http::Header::AccessControlAllowHeaders>("*");
		response.send(Pistache::Http::Code::Ok, "{\"message\":\"success\"}");
	}

// Performance Assessment Handler
	void executePerformanceAssessment(const Pistache::Rest::Request& request,
	                                  Pistache::Http::ResponseWriter response) {
		try {
			Document document;
			document.Parse(request.body().c_str());

			if (document.HasParseError()) {
				response.send(Pistache::Http::Code::Bad_Request, "Invalid JSON format");
				return;
			}

			std::string type, location, practitioner, info, step, comment;

			if (document.HasMember("type")) {
				type = document["type"].GetString();
			}
			if (document.HasMember("location")) {
				location = document["location"].GetString();
			}
			if (document.HasMember("practitioner")) {
				practitioner = document["practitioner"].GetString();
			}
			if (document.HasMember("info")) {
				info = document["info"].GetString();
			}
			if (document.HasMember("step")) {
				step = document["step"].GetString();
			}
			if (document.HasMember("comment")) {
				comment = document["comment"].GetString();
			}

			AMM::UUID erID = SendEventRecord(location, practitioner, type);
			try {
				SendPerformanceAssessment(erID, type, info, step, comment);
			} catch (const std::exception& e) {
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Execution error: " + std::string(e.what()));
				return;
			}

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.headers().add<Pistache::Http::Header::AccessControlAllowHeaders>("*");
			response.send(Pistache::Http::Code::Ok,
			              "{\"message\":\"Performance assessment published\"}");

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in executePerformanceAssessment: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	void executePhysiologyModification(const Pistache::Rest::Request& request,
	                                   Pistache::Http::ResponseWriter response) {
		try {
			Document document;
			document.Parse(request.body().c_str());

			if (document.HasParseError()) {
				response.send(Pistache::Http::Code::Bad_Request, "Invalid JSON format");
				return;
			}

			std::string type, location, practitioner, payload;

			if (!document.HasMember("payload") || !document["payload"].IsString()) {
				response.send(Pistache::Http::Code::Bad_Request,
				              "Missing or invalid 'payload' field in JSON");
				return;
			}

			if (document.HasMember("type")) {
				type = document["type"].GetString();
			}
			if (document.HasMember("location")) {
				location = document["location"].GetString();
			}
			if (document.HasMember("practitioner")) {
				practitioner = document["practitioner"].GetString();
			}
			if (document.HasMember("payload")) {
				payload = document["payload"].GetString();
			}

			AMM::UUID erID = SendEventRecord(location, practitioner, type);
			try {
				SendPhysiologyModification(erID, type, payload);
			} catch (const std::exception& e) {
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Execution error: " + std::string(e.what()));
				return;
			}

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.headers().add<Pistache::Http::Header::AccessControlAllowHeaders>("*");
			response.send(Pistache::Http::Code::Ok,
			              "{\"message\":\"Physiology modification published\"}");

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in executePhysiologyModification: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	void executeRenderModification(const Pistache::Rest::Request& request,
	                               Pistache::Http::ResponseWriter response) {
		try {
			Document document;
			document.Parse(request.body().c_str());

			if (document.HasParseError()) {
				response.send(Pistache::Http::Code::Bad_Request, "Invalid JSON format");
				return;
			}

			std::string type, location, practitioner, payload;

			if (!document.HasMember("payload") || !document["payload"].IsString()) {
				response.send(Pistache::Http::Code::Bad_Request,
				              "Missing or invalid 'payload' field in JSON");
				return;
			}

			if (document.HasMember("type")) {
				type = document["type"].GetString();
			}
			if (document.HasMember("location")) {
				location = document["location"].GetString();
			}
			if (document.HasMember("practitioner")) {
				practitioner = document["practitioner"].GetString();
			}
			if (document.HasMember("payload")) {
				payload = document["payload"].GetString();
			}

			AMM::UUID erID = SendEventRecord(location, practitioner, type);
			try {
				SendRenderModification(erID, type, payload);
			} catch (const std::exception& e) {
				response.send(Pistache::Http::Code::Internal_Server_Error,
				              "Execution error: " + std::string(e.what()));
				return;
			}

			response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
			response.headers().add<Pistache::Http::Header::AccessControlAllowHeaders>("*");
			response.send(Pistache::Http::Code::Ok,
			              "{\"message\":\"Render modification published\"}");

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in executeRenderModification: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error: " + std::string(e.what()));
		}
	}

	void doShutdown(const Pistache::Rest::Request&,
	                Pistache::Http::ResponseWriter response) {
		try {
			m_runThread = false;
			response.cookies().add(Pistache::Http::Cookie("lang", "en-US"));
			response.send(Pistache::Http::Code::Ok, "Shutdown initiated");

			// Give time for the response to be sent before actual shutdown
			std::thread([this]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				if (httpEndpoint) {
					httpEndpoint->shutdown();
				}
			}).detach();

		} catch (const std::exception& e) {
			LOG_ERROR << "Error in doShutdown: " << e.what();
			response.send(Pistache::Http::Code::Internal_Server_Error,
			              "Error during shutdown: " + std::string(e.what()));
		}
	}
};

static void show_usage(const std::string& name) {
	cerr << "Usage: " << name << " <option(s)>\n"
	     << "Options:\n"
	     << "\t-h,--help\t\tShow this help message\n"
	     << "\t-d\t\t\tRun as daemon\n"
	     << "\t-nodiscovery\t\tDisable discovery service\n"
	     << "\tDefault port: 9080\n"
	     << "\tDefault threads: 2\n"
	     << endl;
}

// Main function
int main(int argc, char* argv[]) {
	static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
	plog::init(plog::verbose, &consoleAppender);

	int portNumber = 9080;
	int thr = 2;
	int daemonize = 1;
	int discovery = 1;

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if ((arg == "-h") || (arg == "--help")) {
			show_usage(argv[0]);
			return 0;
		}

		if (arg == "-d") {
			daemonize = 1;
		}

		if (arg == "-nodiscovery") {
			discovery = 0;
		}
	}

	try {
		Pistache::Port port(portNumber);
		Pistache::Address addr(Pistache::Ipv4::any(), port);

		DDSEndpoint server(addr);

		ResetLabs();

		AMMListener al;

		mgr->InitializeCommand();
		mgr->InitializeSimulationControl();
		mgr->InitializePhysiologyValue();
		mgr->InitializeTick();
		mgr->InitializeEventRecord();
		mgr->InitializeRenderModification();
		mgr->InitializePhysiologyModification();
		mgr->InitializeAssessment();
		mgr->InitializeOperationalDescription();
		mgr->InitializeModuleConfiguration();
		mgr->InitializeStatus();

		mgr->CreateTickSubscriber(&al, &AMMListener::onNewTick);
		mgr->CreatePhysiologyValueSubscriber(&al, &AMMListener::onNewPhysiologyValue);
		mgr->CreateCommandSubscriber(&al, &AMMListener::onNewCommand);
		mgr->CreateStatusSubscriber(&al, &AMMListener::onNewStatus);

		mgr->CreateAssessmentPublisher();
		mgr->CreateRenderModificationPublisher();
		mgr->CreatePhysiologyModificationPublisher();
		mgr->CreateSimulationControlPublisher();
		mgr->CreateCommandPublisher();
		mgr->CreateOperationalDescriptionPublisher();
		mgr->CreateModuleConfigurationPublisher();
		mgr->CreateStatusPublisher();

		std::this_thread::sleep_for(std::chrono::milliseconds(250));

		m_uuid.id(mgr->GenerateUuidString());
		PublishOperationalDescription();
		PublishConfiguration();

		gethostname(hostname, HOST_NAME_MAX);

		LOG_INFO << "Listening on *:" << portNumber;
		LOG_INFO << "Hostname is " << hostname;
		LOG_INFO << "Cores = " << Pistache::hardware_concurrency();
		LOG_INFO << "Using " << thr << " threads";

		server.init(thr);
		server.start();

		server.shutdown();

		if (mgr) {
			mgr = nullptr;
		}

		LOG_INFO << "Shutdown complete";
		return 0;

	} catch (const std::exception& e) {
		LOG_ERROR << "Fatal error: " << e.what();
		return 1;
	}
}