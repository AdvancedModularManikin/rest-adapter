
#include <mutex>
#include <chrono>
#include <thread>
#include <iostream>
#include <functional>
#include <condition_variable>

#include "amm_std.h"

#include "AMM/BaseLogger.h"

#include "AMM/Utility.h"

#include <pistache/endpoint.h>
#include <pistache/router.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"

#include "boost/filesystem.hpp"
#include <boost/algorithm/string/join.hpp>

#include "thirdparty/sqlite_modern_cpp.h"

using namespace std;
using namespace std::chrono;
using namespace boost::filesystem;
using namespace rapidjson;
using namespace Pistache;
using namespace eprosima;
using namespace eprosima::fastrtps;
using namespace sqlite;

/// REST adapter port.
int portNumber = 9080;

/// REST threads.
int thr = 2;

/// Daemonize by default.
int daemonize = 1;

/// Default discovery.
int discovery = 1;

/// Hostname to connect to.
char hostname[HOST_NAME_MAX];

std::string action_path = "Actions/";
std::string state_path = "./states/";
std::string patient_path = "./patients/";
std::string scenario_path = "./Scenarios/";

std::map<std::string, double> nodeDataStorage;

std::map<std::string, std::string> statusStorage = {{"STATUS",         "NOT RUNNING"},
                                                    {"TICK",           "0"},
                                                    {"TIME",           "0"},
                                                    {"SCENARIO",       ""},
                                                    {"STATE",          ""},
                                                    {"CLEAR_SUPPLY",   ""},
                                                    {"BLOOD_SUPPLY",   ""},
                                                    {"FLUIDICS_STATE", ""},
                                                    {"IVARM_STATE",    ""}};
std::vector<std::string> labsStorage;

bool m_runThread = false;
int64_t lastTick = 0;

const string sysPrefix = "[SYS]";
const string actPrefix = "[ACT]";
const string loadScenarioPrefix = "LOAD_SCENARIO:";
const string loadScenarioFilePrefix = "LOAD_SCENARIOFILE:";
const string loadPrefix = "LOAD_STATE:";
const string loadPatientPrefix = "LOAD_PATIENT:";

void SendReset();

/// Resets database tables for labs.
void ResetLabs() {
   labsStorage.clear();
   std::ostringstream labRow;

   labRow << "Time,";

// POCT
   labRow << "POCT,";
   labRow << "Sodium (Na),";
   labRow << "Potassium (K),";
   labRow << "Chloride (Cl),";
   labRow << "TCO2,";
   labRow << "Anion Gap,"; // Anion Gap
   labRow << "Ionized Calcium (iCa),"; // Ionized Calcium (iCa)
   labRow << "Glucose (Glu),";
   labRow << "Urea Nitrogen (BUN)/Urea,";
   labRow << "Creatinine (Crea),";

// Hematology
   labRow << "Hematology,";
   labRow << "Hematocrit (Hct),";
   labRow << "Hemoglobin (Hgb),";

//ABG
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
   labRow << "Anion Gap,"; // Anion Gap
   labRow << "Ionized Calcium (iCa),"; // Ionized Calcium (iCa)
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
   labRow << "ALP,"; // ALP
   labRow << "ALT,"; // ALT
   labRow << "AST,"; // AST
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
   labsStorage.push_back(labRow.str());
}

/// Add to database tables for labs.
void AppendLabRow() {
   std::ostringstream labRow;

   labRow << nodeDataStorage["SIM_TIME"] << ",";

// POCT
   labRow << "POCT,";
   labRow << nodeDataStorage["Substance_Sodium"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_Potassium"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_Chloride"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_CarbonDioxide"] << ",";
   labRow << ","; // Anion Gap
   labRow << ","; // Ionized Calcium (iCa)
   labRow << nodeDataStorage["Substance_Glucose_Concentration"] << ",";
   labRow << nodeDataStorage["BloodChemistry_BloodUreaNitrogen_Concentration"] << ",";
   labRow << nodeDataStorage["Substance_Creatinine_Concentration"] << ",";

// Hematology
   labRow << "Hematology,";
   labRow << nodeDataStorage["BloodChemistry_Hemaocrit"] << ",";
   labRow << nodeDataStorage["Substance_Hemoglobin_Concentration"] << ",";

//ABG
   labRow << "ABG,";
   labRow << nodeDataStorage["Substance_Lactate_Concentration_mmol"] << ",";
   labRow << nodeDataStorage["BloodChemistry_BloodPH"] << ",";
   labRow << nodeDataStorage["BloodChemistry_Arterial_CarbonDioxide_Pressure"] << ",";
   labRow << nodeDataStorage["BloodChemistry_Arterial_Oxygen_Pressure"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_CarbonDioxide"] << ",";
   labRow << nodeDataStorage["Substance_Bicarbonate"] << ",";
   labRow << nodeDataStorage["Substance_BaseExcess"] << ",";
   labRow << nodeDataStorage["BloodChemistry_Oxygen_Saturation"] << ",";
   labRow << nodeDataStorage["Substance_Carboxyhemoglobin_Concentration"] << ",";

// VBG
   labRow << "VBG,";
   labRow << nodeDataStorage["Substance_Lactate_Concentration_mmol"] << ",";
   labRow << nodeDataStorage["BloodChemistry_BloodPH"] << ",";
   labRow << nodeDataStorage["BloodChemistry_VenousCarbonDioxidePressure"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_CarbonDioxide"] << ",";
   labRow << nodeDataStorage["Substance_Bicarbonate"] << ",";
   labRow << nodeDataStorage["Substance_BaseExcess"] << ",";
   labRow << nodeDataStorage["Substance_Carboxyhemoglobin_Concentration"] << ",";


   // BMP
   labRow << "BMP,";
   labRow << nodeDataStorage["Substance_Sodium"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_Potassium"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_Chloride"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_CarbonDioxide"] << ",";
   labRow << ","; // Anion Gap
   labRow << ","; // Ionized Calcium (iCa)
   labRow << nodeDataStorage["Substance_Glucose_Concentration"] << ",";
   labRow << nodeDataStorage["BloodChemistry_BloodUreaNitrogen_Concentration"] << ",";
   labRow << nodeDataStorage["Substance_Creatinine_Concentration"] << ",";


   // CBC
   labRow << "CBC,";
   labRow << nodeDataStorage["BloodChemistry_WhiteBloodCell_Count"] << ",";
   labRow << nodeDataStorage["BloodChemistry_RedBloodCell_Count"] << ",";
   labRow << nodeDataStorage["Substance_Hemoglobin_Concentration"] << ",";
   labRow << nodeDataStorage["BloodChemistry_Hemaocrit"] << ",";
   labRow << nodeDataStorage["CompleteBloodCount_Platelet"] << ",";

// CMP
   labRow << "CMP,";
   labRow << nodeDataStorage["Substance_Albumin_Concentration"] << ",";
   labRow << ","; // ALP
   labRow << ","; // ALT
   labRow << ","; // AST
   labRow << nodeDataStorage["BloodChemistry_BloodUreaNitrogen_Concentration"] << ",";
   labRow << nodeDataStorage["Substance_Calcium_Concentration"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_Chloride"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_CarbonDioxide"] << ",";
   labRow << nodeDataStorage["Substance_Creatinine_Concentration"] << ",";
   labRow << nodeDataStorage["Substance_Creatinine_Concentration"] << ",";
   labRow << nodeDataStorage["Substance_Glucose_Concentration"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_Potassium"] << ",";
   labRow << nodeDataStorage["Substance_Sodium"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_Bilirubin"] << ",";
   labRow << nodeDataStorage["MetabolicPanel_Protein"];
   labsStorage.push_back(labRow.str());
}

/// Core logic container for DDS Manager functions.
class AMMListener {
public:

    void onNewStatus(AMM::Status &st, SampleInfo_t *info) {
       ostringstream statusValue;
       statusValue << AMM::Utility::EStatusValueStr(st.value());

       LOG_DEBUG << "[" << st.module_id().id() << "][" << st.module_name() << "]["
                 << st.capability() << "] Status = " << statusValue.str() << " (" << st.value() << ")";

       if (st.module_name() == "AMM_FluidManager" && st.capability() == "") {
          statusStorage["FLUIDICS_STATE"] = statusValue.str();
       }

       if (st.module_name() == "AMM_FluidManager" && st.capability() == "clear_supply") {
          statusStorage["CLEAR_SUPPLY"] = statusValue.str();
       }

       if (st.module_name() == "AMM_FluidManager" && st.capability() == "blood_supply") {
          statusStorage["BLOOD_SUPPLY"] = statusValue.str();
       }

       if (st.capability() == "iv_detection") {
          statusStorage["IVARM_STATE"] = statusValue.str();
       }
    }

    void onNewTick(AMM::Tick &t, SampleInfo_t *info) {
       if (statusStorage["STATUS"].compare("NOT RUNNING") == 0 &&
           t.frame() > lastTick) {
          statusStorage["STATUS"] = "RUNNING";
       }
       lastTick = t.frame();
       statusStorage["TICK"] = to_string(t.frame());
       statusStorage["TIME"] = to_string(t.time());
    }

    void onNewCommand(AMM::Command &c, SampleInfo_t *info) {
       if (!c.message().compare(0, sysPrefix.size(), sysPrefix)) {
          std::string value = c.message().substr(sysPrefix.size());
          if (value.compare("START_SIM") == 0) {
             statusStorage["STATUS"] = "RUNNING";
          } else if (value.compare("STOP_SIM") == 0) {
             statusStorage["STATUS"] = "STOPPED";
          } else if (value.compare("PAUSE_SIM") == 0) {
             statusStorage["STATUS"] = "PAUSED";
          } else if (value.compare("RESET_SIM") == 0) {
             statusStorage["STATUS"] = "NOT RUNNING";
             statusStorage["TICK"] = "0";
             statusStorage["TIME"] = "0";
             nodeDataStorage.clear();
             ResetLabs();
          } else if (value.compare("APPEND_LABS") == 0) {
             AppendLabRow();
          } else if (value.compare("CLEAR_LOG") == 0) {

          } else if (!value.compare(0, loadPrefix.size(), loadPrefix)) {
             statusStorage["STATE"] = value.substr(loadPrefix.size());
             SendReset();
          } else if (!value.compare(0, loadScenarioPrefix.size(),
                                    loadScenarioPrefix)) {
             statusStorage["SCENARIO"] = value.substr(loadScenarioPrefix.size());
          } else if (!value.compare(0, loadPatientPrefix.size(),
                                     loadPatientPrefix)) {
           statusStorage["PATIENT"] = value.substr(loadPatientPrefix.size());
            }
       }
    }

    void onNewPhysiologyValue(AMM::PhysiologyValue &n, SampleInfo_t *info) {
       if (!isnan(n.value())) {
          nodeDataStorage[n.name()] = n.value();
       }
    }
};

const std::string moduleName = "AMM_REST_Adapter";
const std::string configFile = "config/rest_adapter_amm.xml";
AMM::DDSManager<AMMListener> *mgr = new AMM::DDSManager<AMMListener>(configFile);
AMM::UUID m_uuid;
database db("amm.db");

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
   LOG_DEBUG << "Publishing a phys mod: " << type;
   AMM::PhysiologyModification modInstance;
   AMM::UUID instUUID;
   instUUID.id(mgr->GenerateUuidString());
   modInstance.id(instUUID);
   modInstance.type(type);
   modInstance.event_id(er_id);
   modInstance.data(payload);
   mgr->WritePhysiologyModification(modInstance);
}

void SendRenderModification(AMM::UUID &er_id,
                            const std::string &type,
                            const std::string &payload) {
   LOG_DEBUG << "Publishing a render mod: " << type;
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
   LOG_INFO << "Publishing an assessment: " << assessment_type;
   AMM::Assessment assessInstance;
   AMM::UUID instUUID;
   instUUID.id(mgr->GenerateUuidString());
   assessInstance.id(instUUID);
   assessInstance.event_id(er_id);
   assessInstance.comment(comment);
   mgr->WriteAssessment(assessInstance);
}

void SendCommand(const std::string &command) {
   LOG_INFO << "Publishing a command:" << command;
   if (!command.compare(0, sysPrefix.size(), sysPrefix)) {
      std::string value = command.substr(sysPrefix.size());
      if (value.compare("START_SIM") == 0) {
         AMM::SimulationControl simControl;
         auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
         simControl.timestamp(ms);
         simControl.type(AMM::ControlType::RUN);
         mgr->WriteSimulationControl(simControl);
         statusStorage["STATUS"] = "RUNNING";
      } else if (value.compare("STOP_SIM") == 0) {
         AMM::SimulationControl simControl;
         auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
         simControl.timestamp(ms);
         simControl.type(AMM::ControlType::HALT);
         mgr->WriteSimulationControl(simControl);
         statusStorage["STATUS"] = "STOPPED";
      } else if (value.compare("PAUSE_SIM") == 0) {
         AMM::SimulationControl simControl;
         auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
         simControl.timestamp(ms);
         simControl.type(AMM::ControlType::HALT);
         mgr->WriteSimulationControl(simControl);
         statusStorage["STATUS"] = "PAUSED";
      } else if (value.compare("RESET_SIM") == 0) {
         statusStorage["STATUS"] = "NOT RUNNING";
         statusStorage["TICK"] = "0";
         statusStorage["TIME"] = "0";
         nodeDataStorage.clear();
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
      // Publish some other command?
      AMM::Command cmdInstance;
      cmdInstance.message(command);
      mgr->WriteCommand(cmdInstance);
   }
}

void printCookies(const Http::Request &req) {
   auto cookies = req.cookies();
   std::cout << "Cookies: [" << std::endl;
   const std::string indent(4, ' ');
   for (const auto &c : cookies) {
      std::cout << indent << c.name << " = " << c.value << std::endl;
   }
   std::cout << "]" << std::endl;
}

namespace Generic {

    void handleReady(const Rest::Request &request, Http::ResponseWriter response) {
       response.send(Http::Code::Ok, "1");
    }
}

class DDSEndpoint {
    static constexpr size_t DefaultMaxPayload = 1024102410;

public:
    explicit DDSEndpoint(Address addr)
       : httpEndpoint(std::make_shared<Http::Endpoint>(addr)) {}

    void init(int thr = 2) {
       auto opts = Http::Endpoint::options()
          .threads(thr)
          .flags(Tcp::Options::ReuseAddr);
       httpEndpoint->init(opts);
       setupRoutes();
    }

    void start() {
       httpEndpoint->setHandler(router.handler());
       httpEndpoint->serveThreaded();
    }

    void shutdown() { httpEndpoint->shutdown(); }

private:
    void setupRoutes() {
       using namespace Rest;

       Routes::Get(router, "/instance", Routes::bind(&DDSEndpoint::getInstance, this));
       Routes::Get(router, "/node/:name", Routes::bind(&DDSEndpoint::getNode, this));
       Routes::Get(router, "/nodes", Routes::bind(&DDSEndpoint::getNodes, this));
       Routes::Get(router, "/command/:name",
                   Routes::bind(&DDSEndpoint::issueCommand, this));
       Routes::Get(router, "/ready", Routes::bind(&Generic::handleReady));
       Routes::Get(router, "/debug", Routes::bind(&DDSEndpoint::doDebug, this));

       Routes::Get(router, "/labs", Routes::bind(&DDSEndpoint::getLabsReport, this));

       Routes::Get(router, "/events",
                   Routes::bind(&DDSEndpoint::getEventLog, this));
       Routes::Get(router, "/events/csv",
                   Routes::bind(&DDSEndpoint::getEventLogCSV, this));

       Routes::Get(router, "/logs",
                   Routes::bind(&DDSEndpoint::getDiagnosticLog, this));

       Routes::Get(router, "/logs/csv",
                   Routes::bind(&DDSEndpoint::getDiagnosticLogCSV, this));

       Routes::Get(router, "/modules/count",
                   Routes::bind(&DDSEndpoint::getModuleCount, this));
       Routes::Get(router, "/modules",
                   Routes::bind(&DDSEndpoint::getModules, this));
       Routes::Get(router, "/module/id/:id",
                   Routes::bind(&DDSEndpoint::getModuleById, this));
       Routes::Get(router, "/module/guid/:guid",
                   Routes::bind(&DDSEndpoint::getModuleByGuid, this));

       Routes::Get(router, "/modules/other",
                   Routes::bind(&DDSEndpoint::getOtherModules, this));

       Routes::Get(router, "/shutdown",
                   Routes::bind(&DDSEndpoint::doShutdown, this));

       Routes::Get(router, "/actions",
                   Routes::bind(&DDSEndpoint::getActions, this));
       Routes::Get(router, "/action/:name",
                   Routes::bind(&DDSEndpoint::getAction, this));
       Routes::Post(router, "/action",
                    Routes::bind(&DDSEndpoint::createAction, this));
       Routes::Put(router, "/action/:name",
                   Routes::bind(&DDSEndpoint::updateAction, this));
       Routes::Delete(router, "/action/:name",
                      Routes::bind(&DDSEndpoint::deleteAction, this));

        Routes::Get(router, "/assessments",
                    Routes::bind(&DDSEndpoint::getAssessments, this));
        Routes::Get(router, "/assessment/:name",
                    Routes::bind(&DDSEndpoint::getAssessment, this));
        Routes::Post(router, "/assessment/:name",
                     Routes::bind(&DDSEndpoint::createAssessment, this));
        Routes::Put(router, "/assessment/:name",
                     Routes::bind(&DDSEndpoint::createAssessment, this));
        Routes::Delete(router, "/assessment/:name",
                       Routes::bind(&DDSEndpoint::deleteAssessment, this));

       Routes::Post(router, "/execute",
                    Routes::bind(&DDSEndpoint::executeCommand, this));
       Routes::Options(router, "/execute",
                       Routes::bind(&DDSEndpoint::executeOptions, this));

       Routes::Post(
          router, "/topic/physiology_modification",
          Routes::bind(&DDSEndpoint::executePhysiologyModification, this));
       Routes::Post(router, "/topic/render_modification",
                    Routes::bind(&DDSEndpoint::executeRenderModification, this));
       Routes::Post(
          router, "/topic/performance_assessment",
          Routes::bind(&DDSEndpoint::executePerformanceAssessment, this));
       Routes::Options(router, "/topic/:mod_type",
                       Routes::bind(&DDSEndpoint::executeOptions, this));

       Routes::Get(router, "/patients",
                   Routes::bind(&DDSEndpoint::getPatients, this));

        Routes::Get(router, "/scenarios",
                    Routes::bind(&DDSEndpoint::getScenarios, this));

       Routes::Get(router, "/states", Routes::bind(&DDSEndpoint::getStates, this));
       Routes::Get(router, "/states/:name/delete",
                   Routes::bind(&DDSEndpoint::deleteState, this));
    }

    void getInstance(const Rest::Request &request,
                     Http::ResponseWriter response) {
       StringBuffer s;
       Writer<StringBuffer> writer(s);

       std::ifstream t("static/current_scenario.txt");
       std::string scenario((std::istreambuf_iterator<char>(t)),
                            std::istreambuf_iterator<char>());
       t.close();

       writer.StartObject();
       writer.Key("name");
       writer.String(hostname);
       writer.Key("scenario");
       writer.String(scenario.c_str());
       writer.EndObject();

       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getStates(const Rest::Request &request, Http::ResponseWriter response) {

       StringBuffer s;
       Writer<StringBuffer> writer(s);

       writer.StartArray();
       if (exists(state_path) && is_directory(state_path)) {
          path p(state_path);
          if (is_directory(p)) {
             std::vector<boost::filesystem::path> paths(
                boost::filesystem::directory_iterator{state_path}, boost::filesystem::directory_iterator{}
             );
             std::sort(paths.begin(), paths.end());
             for (auto const &path : paths) {
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

       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void deleteState(const Rest::Request &request,
                     Http::ResponseWriter response) {
       auto name = request.param(":name").as<std::string>();
       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       if (name != "StandardMale@0s.xml") {
          std::ostringstream deleteFile;
          deleteFile << state_path << "/" << name;
          path deletePath(deleteFile.str().c_str());
          if (exists(deletePath) && is_regular_file(deletePath)) {
             LOG_INFO << "Deleting " << deletePath;
             boost::filesystem::remove(deletePath);
             response.send(Pistache::Http::Code::Ok, "Deleted",
                           MIME(Application, Json));
          } else {
             response.send(Pistache::Http::Code::Forbidden,
                           "Unable to delete state file", MIME(Application, Json));
          }
       } else {
          response.send(Pistache::Http::Code::Forbidden,
                        "Can not delete default state file",
                        MIME(Application, Json));
       }
    }

    void getScenarios(const Rest::Request &request,
                     Http::ResponseWriter response) {
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

        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Pistache::Http::Code::Ok, s.GetString(),
                      MIME(Application, Json));
    }

    void getPatients(const Rest::Request &request,
                     Http::ResponseWriter response) {
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

       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Pistache::Http::Code::Ok, s.GetString(),
                     MIME(Application, Json));
    }

    void getAssessments(const Rest::Request &request, Http::ResponseWriter response) {
        LOG_INFO << "Get a list of all assessment CSVs";

    }

    int writeToFile(const string & filename, const string & data) {
        std::fstream out;

        out.open(filename, std::fstream::in | std::fstream::out | std::fstream::trunc);

        LOG_INFO << "Writing to " << filename;

        // If file does not exist, Create new file
        if (!out)
        {
            LOG_WARNING << "Cannot open file, file does not exist. Creating new file.";
            out.open(filename);
        }

        if (out.fail()) {
            LOG_ERROR << "Open failure: " << strerror(errno);
            return -1;
        }

        out << data;
        out.close();

        LOG_INFO << "All done";
    }

    void createAssessment(const Rest::Request &request,
                      Http::ResponseWriter response) {
        auto name = request.param(":name").as<std::string>();
        if (name.empty()) {
            LOG_INFO << "Name was empty";
            name = "test.csv";
        }

        std::string filename = "assessments/" + name;
        LOG_INFO << "Create an assessment from a POST.  Filename is " << filename;
        auto s = std::chrono::steady_clock::now();
        writeToFile(filename, request.body());
        auto dt = std::chrono::steady_clock::now() - s;
        LOG_INFO << "File upload processed in: " << std::chrono::duration_cast<std::chrono::microseconds>(dt).count()<< endl;
        response.send(Http::Code::Ok);

        LOG_INFO << "Sending out system message that there's an assessment available.";
        std::string command = "[SYS]ASSESSMENT_AVAILABLE:" + name;
        AMM::Command cmdInstance;
        cmdInstance.message(command);
        mgr->WriteCommand(cmdInstance);
    }

    void deleteAssessment(const Rest::Request &request,
                      Http::ResponseWriter response) {
        auto name = request.param(":name").as<std::string>();
    }

    void getAssessment(const Rest::Request &request, Http::ResponseWriter response) {
        std::string name = request.param(":name").as<std::string>();
        std::string fileName = "assessments/" + name;
        LOG_INFO << "Recieved a request to GET an assessment file, so we're serving this up: " << fileName;
        Http::serveFile(response, fileName.c_str());
    }



    void getActions(const Rest::Request &request, Http::ResponseWriter response) {
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

       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Pistache::Http::Code::Ok, s.GetString(),
                     MIME(Application, Json));
    }

    void executeCommand(const Rest::Request &request,
                        Http::ResponseWriter response) {
       Document document;
       document.Parse(request.body().c_str());
       std::string payload = document["payload"].GetString();

       SendCommand(payload);

       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.headers().add<Http::Header::AccessControlAllowHeaders>("*");
       response.send(Pistache::Http::Code::Ok,
                     "{\"message\":\"Command executed\"}");
    }

    void executePhysiologyModification(const Rest::Request &request,
                                       Http::ResponseWriter response) {
       std::string type, location, practitioner, payload;
       Document document;
       document.Parse(request.body().c_str());
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
       SendPhysiologyModification(erID, type, payload);
       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.headers().add<Http::Header::AccessControlAllowHeaders>("*");
       response.send(Pistache::Http::Code::Ok,
                     "{\"message\":\"Physiology modification published\"}");
    }

    void executeRenderModification(const Rest::Request &request,
                                   Http::ResponseWriter response) {
       std::string type, location, practitioner, payload;
       Document document;
       document.Parse(request.body().c_str());
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
       SendRenderModification(erID, type, payload);
       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.headers().add<Http::Header::AccessControlAllowHeaders>("*");
       response.send(Pistache::Http::Code::Ok,
                     "{\"message\":\"Render modification published\"}");
    }

    void executePerformanceAssessment(const Rest::Request &request,
                                      Http::ResponseWriter response) {
       std::string type, location, practitioner, payload, info, step, comment;
       Document document;
       document.Parse(request.body().c_str());
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
       SendPerformanceAssessment(erID, type, info, step, comment);
       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.headers().add<Http::Header::AccessControlAllowHeaders>("*");
       response.send(Pistache::Http::Code::Ok,
                     "{\"message\":\"Performance assessment published\"}");
    }

    void executeOptions(const Rest::Request &request,
                        Http::ResponseWriter response) {
       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.headers().add<Http::Header::AccessControlAllowHeaders>("*");
       response.send(Pistache::Http::Code::Ok, "{\"message\":\"success\"}");
    }

    void createAction(const Rest::Request &request,
                      Http::ResponseWriter response) {}

    void deleteAction(const Rest::Request &request,
                      Http::ResponseWriter response) {
       auto name = request.param(":name").as<std::string>();
    }

    void updateAction(const Rest::Request &request,
                      Http::ResponseWriter response) {
       auto name = request.param(":name").as<std::string>();
    }

    void getAction(const Rest::Request &request, Http::ResponseWriter response) {
       auto name = request.param(":name").as<std::string>();
    }

    void issueCommand(const Rest::Request &request,
                      Http::ResponseWriter response) {
       auto name = request.param(":name").as<std::string>();
       SendCommand(name);
       StringBuffer s;
       Writer<StringBuffer> writer(s);
       writer.StartObject();
       writer.Key("Sent command");
       writer.String(name.c_str());
       writer.EndObject();
       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, s.GetString());
    }

    void getModuleById(const Rest::Request &request,
                       Http::ResponseWriter response) {
       auto id = request.param(":id").as<std::string>();
       StringBuffer s;
       Writer<StringBuffer> writer(s);
       db << "SELECT "
             "module_id AS module_id,"
             "module_name AS module_name,"
             "description AS description,"
             "capabilities as capabilities,"
             "manufacturer as manufacturer,"
             "model as model "
             " FROM "
             " module_capabilities "
             " WHERE module_id = ?"
          << id >>
          [&](string module_id, string module_guid, string module_name, string description,
              string capabilities, string manufacturer, string model) {
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

       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getModuleByGuid(const Rest::Request &request,
                         Http::ResponseWriter response) {
       auto guid = request.param(":guid").as<std::string>();
       StringBuffer s;
       Writer<StringBuffer> writer(s);
       db << "SELECT "
             "module_id AS module_id,"
             "module_guid as module_guid,"
             "module_name AS module_name,"
             "capabilities as capabilities,"
             "manufacturer as manufacturer,"
             "model as model "
             " FROM "
             " module_capabilities "
             " WHERE module_guid = ?"
          << guid >>
          [&](string module_id, string module_guid, string module_name,
              string capabilities, string manufacturer, string model) {
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
          };

       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getModuleCount(const Rest::Request &request,
                        Http::ResponseWriter response) {
       StringBuffer s;
       Writer<StringBuffer> writer(s);

       int totalCount = 0;
       int coreCount = 0;
       int otherCount = 0;

       db << "SELECT COUNT(DISTINCT module_name) FROM module_capabilities" >> totalCount;
       db << "SELECT COUNT(DISTINCT module_name) FROM module_capabilities where module_name LIKE 'AMM_%'" >> coreCount;

       otherCount = totalCount - coreCount;

       writer.StartObject();
       writer.Key("module_count");
       writer.Int(totalCount);
       writer.Key("core_count");
       writer.Int(coreCount);
       writer.Key("other_count");
       writer.Int(otherCount);
       writer.EndObject();

       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getOtherModules(const Rest::Request &request, Http::ResponseWriter response) {
       StringBuffer s;
       Writer<StringBuffer> writer(s);
       writer.StartArray();
       db << "SELECT DISTINCT module_name FROM module_capabilities where module_name NOT LIKE 'AMM_%'"
          >> [&](string module_name) {
              writer.String(module_name.c_str());
          };
       writer.EndArray();
       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getModules(const Rest::Request &request, Http::ResponseWriter response) {
       StringBuffer s;
       Writer<StringBuffer> writer(s);
       writer.StartArray();

       db << "SELECT "
             "module_capabilities.module_id AS module_id,"
             "module_capabilities.module_name AS module_name,"
             "module_capabilities.description AS description,"
             "module_capabilities.capabilities as capabilities,"
             "module_capabilities.manufacturer as manufacturer,"
             "module_capabilities.model as model "
             " FROM "
             " module_capabilities; " >>
          [&](string module_id, string module_name, string description,
              string capabilities,
              string manufacturer, string model) {
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

       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getEventLog(const Rest::Request &request,
                     Http::ResponseWriter response) {
       StringBuffer s;
       Writer<StringBuffer> writer(s);
       writer.StartArray();

       db << "SELECT "
             "module_capabilities.module_id as module_id,"
             "module_capabilities.module_name as module_name,"
             "events.source,"
             "events.topic,"
             "events.timestamp,"
             "events.data "
             "FROM "
             "events "
             "LEFT JOIN module_capabilities "
             "ON "
             "events.source = module_capabilities.module_guid" >>
          [&](string module_id, string module_name, string source, string topic, int64_t timestamp, string data) {
              writer.StartObject();
              writer.Key("module_id");
              writer.String(module_id.c_str());
              writer.Key("source");
              writer.String(module_name.c_str());
              writer.Key("module_guid");
              writer.String(source.c_str());
              writer.Key("timestamp");
              writer.Uint64(timestamp);
              writer.Key("topic");
              writer.String(topic.c_str());
              writer.Key("message");
              writer.String(data.c_str());
              writer.EndObject();
          };

       writer.EndArray();
       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getEventLogCSV(const Rest::Request &request,
                        Http::ResponseWriter response) {
       std::ostringstream s;

       db << "SELECT "
             "module_capabilities.module_name,"
             "events.source,"
             "events.topic,"
             "events.tick,"
             "events.timestamp,"
             "events.data "
             "FROM "
             "events "
             "LEFT JOIN module_capabilities "
             "ON "
             "events.source = module_capabilities.module_guid" >>
          [&](string module_name, string source, string topic, int64_t tick, int64_t timestamp,
              string data) {
              std::time_t temp = timestamp;
              std::tm *t = std::gmtime(&temp);
              s << std::put_time(t, "%Y-%m-%d %I:%M:%S %p") << "," << module_name << "," << source << "," << topic
                << "," << data << std::endl;
          };


       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       auto csvHeader = Http::Header::Raw("Content-Disposition", "attachment;filename=amm_timeline_log.csv");
       response.headers().addRaw(csvHeader);
       response.send(Http::Code::Ok, s.str(), Http::Mime::MediaType::fromString("text/csv"));
    }

    void getDiagnosticLog(const Rest::Request &request,
                          Http::ResponseWriter response) {
       StringBuffer s;
       Writer<StringBuffer> writer(s);
       writer.StartArray();

       db << "SELECT "
             "logs.module_name, "
             "logs.module_guid, "
             "logs.module_id, "
             "logs.message,"
             "logs.log_level,"
             "logs.timestamp "
             "FROM "
             "logs " >>
          [&](string module_name, string module_guid, string module_id, string message, string log_level,
              int64_t timestamp) {

              writer.StartObject();
              writer.Key("source");
              writer.String(module_name.c_str());
              writer.Key("module_guid");
              writer.String(module_guid.c_str());
              writer.Key("module_id");
              writer.String(module_id.c_str());
              writer.Key("timestamp");
              writer.Uint64(timestamp);
              writer.Key("log_level");
              writer.String(log_level.c_str());
              writer.Key("message");
              writer.String(message.c_str());
              writer.EndObject();
          };

       writer.EndArray();
       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getDiagnosticLogCSV(const Rest::Request &request,
                             Http::ResponseWriter response) {
       std::ostringstream s;

       db << "SELECT "
             "logs.module_name, "
             "logs.module_guid, "
             "logs.module_id, "
             "logs.message,"
             "logs.log_level,"
             "logs.timestamp "
             "FROM "
             "logs " >>
          [&](string module_name, string module_guid, string module_id, string message, string log_level,
              int64_t timestamp) {
              std::time_t temp = timestamp;
              std::tm *t = std::gmtime(&temp);
              s << std::put_time(t, "%Y-%m-%d %I:%M:%S %p") << "," << log_level << "," << module_name << "," << message
                << std::endl;
          };

       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       auto csvHeader = Http::Header::Raw("Content-Disposition", "attachment;filename=amm_diagnostic_log.csv");
       response.headers().addRaw(csvHeader);
       response.send(Http::Code::Ok, s.str(), Http::Mime::MediaType::fromString("text/csv"));
    }

    void getNodes(const Rest::Request &request, Http::ResponseWriter response) {
       StringBuffer s;
       Writer<StringBuffer> writer(s);
       writer.StartArray();

       auto nit = nodeDataStorage.begin();
       while (nit != nodeDataStorage.end()) {
          writer.StartObject();
          writer.Key(nit->first.c_str());
          writer.Double(nit->second);
          writer.EndObject();
          ++nit;
       }

       auto sit = statusStorage.begin();
       while (sit != statusStorage.end()) {
          writer.StartObject();
          writer.Key(sit->first.c_str());
          writer.String(sit->second.c_str());
          writer.EndObject();
          ++sit;
       }

       writer.EndArray();
       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getLabsReport(const Rest::Request &request, Http::ResponseWriter response) {
       std::string labReport = boost::algorithm::join(labsStorage, "\n");
       auto mime = Http::Mime::MediaType::fromString("text/csv");
       response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
       response.send(Http::Code::Ok, labReport, mime);
    }

    void getNode(const Rest::Request &request, Http::ResponseWriter response) {

       auto name = request.param(":name").as<std::string>();
       auto it = nodeDataStorage.find(name);
       if (it != nodeDataStorage.end()) {
          StringBuffer s;
          Writer<StringBuffer> writer(s);
          writer.StartObject();
          writer.Key(it->first.c_str());
          writer.Double(it->second);
          writer.EndObject();
          response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
          response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
       } else {
          response.send(Http::Code::Not_Found, "Node data does not exist");
       }
    }

    void doDebug(const Rest::Request &request, Http::ResponseWriter response) {
       printCookies(request);
       response.cookies().add(Http::Cookie("lang", "en-US"));
       response.send(Http::Code::Ok);
    }

    void doShutdown(const Rest::Request &request, Http::ResponseWriter response) {
       m_runThread = false;
       response.cookies().add(Http::Cookie("lang", "en-US"));
       response.send(Http::Code::Ok);
    }

    typedef std::mutex Lock;
    typedef std::lock_guard<Lock> Guard;
    Lock commandLock;

    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;
};


static void show_usage(const std::string &name) {
   cerr << "Usage: " << name << " <option(s)>"
        << "\nOptions:\n"
        << "\t-h,--help\t\tShow this help message\n"
        << endl;
}

Port port(static_cast<uint16_t>(portNumber));
Address addr(Ipv4::any(), port);
DDSEndpoint server(addr);

int main(int argc, char *argv[]) {
   static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
   plog::init(plog::verbose, &consoleAppender);

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

   string action;


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

   m_uuid.id(mgr->GenerateUuidString());

   std::this_thread::sleep_for(std::chrono::milliseconds(250));

   PublishOperationalDescription();
   PublishConfiguration();

   gethostname(hostname, HOST_NAME_MAX);

   server.init(thr);
   LOG_INFO << "Listening on *:" << portNumber;

   m_runThread = true;

   server.start();

   LOG_INFO << "Ready.";

   while (m_runThread) {
      getline(cin, action);
      transform(action.begin(), action.end(), action.begin(), ::toupper);
      if (action == "EXIT") {
         m_runThread = false;
         LOG_INFO << "Shutting down from command-line.";
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      cout.flush();
   }

   server.shutdown();

   LOG_INFO << "Shutdown complete";

   return 0;
}
