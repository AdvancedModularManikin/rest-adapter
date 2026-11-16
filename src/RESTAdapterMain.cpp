#include <mutex>
#include <atomic>
#include <csignal>
#include <cctype>
#include <chrono>
#include <thread>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <functional>
#include <condition_variable>
#include <stdexcept>

#include "amm_std.h"

#include "amm/BaseLogger.h"

#include "amm/Utility.h"

#include <pistache/endpoint.h>
#include <pistache/router.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"

#include "boost/filesystem.hpp"
#include <boost/algorithm/string/join.hpp>
#include <boost/process.hpp>

#include "thirdparty/sqlite_modern_cpp.h"

// Undefine syslog integer macros that conflict with plog's LOG_* stream macros.
// On some systems (e.g. Jetson Ubuntu), DDS/system headers pull in <syslog.h>
// after plog, redefining LOG_DEBUG/LOG_INFO/etc. as integers and breaking <<.
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_WARNING
#undef LOG_WARNING
#endif
#ifdef LOG_ERROR
#undef LOG_ERROR
#endif
#ifdef LOG_FATAL
#undef LOG_FATAL
#endif
// Restore plog stream macros using LOG() — plog's internal base macro.
// (Older plog versions don't define PLOG; LOG(severity) is the stable form.)
#define LOG_DEBUG   LOG(plog::debug)
#define LOG_INFO    LOG(plog::info)
#define LOG_WARNING LOG(plog::warning)
#define LOG_ERROR   LOG(plog::error)
#define LOG_FATAL   LOG(plog::fatal)

using namespace AMM;
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
std::string assessment_path = "./assessments/";
std::string state_path = "./states/";
std::string patient_path = "./patients/";
std::string scenario_path = "./static/scenarios/";

std::mutex nds_mutex;
std::mutex status_mutex;
std::mutex labs_mutex;
std::mutex db_mutex;
std::mutex shutdown_mutex;
std::condition_variable shutdown_cv;

std::map<std::string, double> nodeDataStorage;

std::map<std::string, std::string> statusStorage = {
    {"STATUS", "NOT RUNNING"},
    {"TICK", "0"},
    {"TIME", "0"},
    {"SCENARIO", ""},
    {"STATE", ""},
    {"AIR_SUPPLY", ""},
    {"CLEAR_SUPPLY", ""},
    {"BLOOD_SUPPLY", ""},
    {"FLUIDICS_STATE", ""},
    {"BATTERY1", ""},
    {"BATTERY2", ""},
    {"EXT_POWER", ""},
    {"IVARM_STATE", ""},
    {"MONITOR_ECG", ""},
    {"MONITOR_PULSEOX", ""},
    {"MONITOR_NIBP", ""},
    {"MONITOR_TEMP", ""},
    {"MONITOR_ARTLINE", ""},
    {"MONITOR_ETCO2", ""},
};
std::vector<std::string> labsStorage;

std::atomic<bool> m_runThread{false};
int64_t lastTick = 0;

const string sysPrefix = "[SYS]";
const string actPrefix = "[ACT]";
const string loadScenarioPrefix = "LOAD_SCENARIO:";
const string loadPrefix = "LOAD_STATE:";
const string loadPatientPrefix = "LOAD_PATIENT:";

void SendReset();

std::string urlDecode(const std::string &str)
{
    std::string decoded;
    decoded.reserve(str.size());
    for (std::size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '%' && i + 2 < str.size() &&
            std::isxdigit(static_cast<unsigned char>(str[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(str[i + 2])))
        {
            int val = std::stoi(str.substr(i + 1, 2), nullptr, 16);
            decoded.push_back(static_cast<char>(val));
            i += 2;
        }
        else if (str[i] == '+')
        {
            decoded.push_back(' ');
        }
        else
        {
            decoded.push_back(str[i]);
        }
    }
    return decoded;
}

bool isValidFilename(const std::string &name)
{
    if (name.empty() || name == "." || name == ".." ||
        name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos ||
        name.find("..") != std::string::npos ||
        name.find('\0') != std::string::npos)
    {
        return false;
    }
    return path(name).filename().string() == name;
}

std::string escapeCsvField(const std::string &field)
{
    if (field.find_first_of(",\"\r\n") == std::string::npos)
    {
        return field;
    }
    std::string escaped = "\"";
    for (char c : field)
    {
        if (c == '"')
        {
            escaped += "\"\"";
        }
        else
        {
            escaped.push_back(c);
        }
    }
    escaped += "\"";
    return escaped;
}

std::string formatTimestampCsv(int64_t timestamp)
{
    std::time_t temp = (timestamp > 10000000000LL) ? (timestamp / 1000) : timestamp;
    std::tm tm_buf{};
#if defined(_WIN32)
    bool ok = (gmtime_s(&tm_buf, &temp) == 0);
#else
    bool ok = (gmtime_r(&temp, &tm_buf) != nullptr);
#endif
    std::ostringstream oss;
    if (ok)
    {
        oss << std::put_time(&tm_buf, "%Y-%m-%d %I:%M:%S %p");
    }
    else
    {
        oss << timestamp;
    }
    return oss.str();
}

std::string getJsonString(const Document &doc, const char *key)
{
    if (doc.HasMember(key) && doc[key].IsString())
    {
        return doc[key].GetString();
    }
    return {};
}

/// Resets database tables for labs.
void ResetLabs()
{
    const std::lock_guard<std::mutex> lock(labs_mutex);
    labsStorage.clear();
    std::ostringstream labRow;

    labRow << "Time,";

    // POCT
    labRow << "POCT,";
    labRow << "Sodium (Na),";
    labRow << "Potassium (K),";
    labRow << "Chloride (Cl),";
    labRow << "TCO2,";
    labRow << "Anion Gap,";             // Anion Gap
    labRow << "Ionized Calcium (iCa),"; // Ionized Calcium (iCa)
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
    labRow << "modified_pH,";
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
    labRow << "Anion Gap,";             // Anion Gap
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
void AppendLabRow()
{
    std::ostringstream labRow;

    // Take a snapshot of all needed values while holding the lock
    std::map<std::string, double> nodeSnapshot;
    {
        const std::lock_guard<std::mutex> lock(nds_mutex);
        nodeSnapshot = nodeDataStorage;
    }

    auto getVal = [&nodeSnapshot](const std::string &key) -> double
    {
        auto it = nodeSnapshot.find(key);
        return (it != nodeSnapshot.end()) ? it->second : 0.0;
    };

    auto simTimeIt = nodeSnapshot.find("SIM_TIME");
    if (simTimeIt != nodeSnapshot.end())
    {
        labRow << simTimeIt->second << ",";
    }
    else
    {
        std::string fallbackTime;
        {
            const std::lock_guard<std::mutex> lock(status_mutex);
            fallbackTime = statusStorage["TIME"];
        }
        labRow << (fallbackTime.empty() ? "0" : fallbackTime) << ",";
    }

    // POCT
    labRow << "POCT,";
    labRow << getVal("Substance_Sodium") << ",";
    labRow << getVal("MetabolicPanel_Potassium") << ",";
    labRow << getVal("MetabolicPanel_Chloride") << ",";
    labRow << getVal("MetabolicPanel_CarbonDioxide") << ",";
    labRow << ","; // Anion Gap
    labRow << ","; // Ionized Calcium (iCa)
    labRow << getVal("Substance_Glucose_Concentration") << ",";
    labRow << getVal("BloodChemistry_BloodUreaNitrogen_Concentration") << ",";
    labRow << getVal("Substance_Creatinine_Concentration") << ",";

    // Hematology
    labRow << "Hematology,";
    labRow << getVal("BloodChemistry_Hemaocrit") << ",";
    labRow << getVal("Substance_Hemoglobin_Concentration") << ",";

    // ABG
    labRow << "ABG,";
    labRow << getVal("Substance_Lactate_Concentration_mmol") << ",";
    labRow << getVal("BloodChemistry_BloodPH") << ",";
    labRow << getVal("BloodChemistry_BloodPH_MOD") << ",";
    labRow << getVal("BloodChemistry_Arterial_CarbonDioxide_Pressure") << ",";
    labRow << getVal("BloodChemistry_Arterial_Oxygen_Pressure") << ",";
    labRow << getVal("MetabolicPanel_CarbonDioxide") << ",";
    labRow << getVal("Substance_Bicarbonate") << ",";
    labRow << getVal("Substance_BaseExcess") << ",";
    labRow << getVal("BloodChemistry_Oxygen_Saturation") << ",";
    labRow << getVal("Substance_Carboxyhemoglobin_Concentration") << ",";

    // VBG
    labRow << "VBG,";
    labRow << getVal("Substance_Lactate_Concentration_mmol") << ",";
    labRow << getVal("BloodChemistry_BloodPH") << ",";
    labRow << getVal("BloodChemistry_VenousCarbonDioxidePressure") << ",";
    labRow << getVal("MetabolicPanel_CarbonDioxide") << ",";
    labRow << getVal("Substance_Bicarbonate") << ",";
    labRow << getVal("Substance_BaseExcess") << ",";
    labRow << getVal("Substance_Carboxyhemoglobin_Concentration") << ",";

    // BMP
    labRow << "BMP,";
    labRow << getVal("Substance_Sodium") << ",";
    labRow << getVal("MetabolicPanel_Potassium") << ",";
    labRow << getVal("MetabolicPanel_Chloride") << ",";
    labRow << getVal("MetabolicPanel_CarbonDioxide") << ",";
    labRow << ","; // Anion Gap
    labRow << ","; // Ionized Calcium (iCa)
    labRow << getVal("Substance_Glucose_Concentration") << ",";
    labRow << getVal("BloodChemistry_BloodUreaNitrogen_Concentration") << ",";
    labRow << getVal("Substance_Creatinine_Concentration") << ",";

    // CBC
    labRow << "CBC,";
    labRow << getVal("BloodChemistry_WhiteBloodCell_Count") << ",";
    labRow << getVal("BloodChemistry_RedBloodCell_Count") << ",";
    labRow << getVal("Substance_Hemoglobin_Concentration") << ",";
    labRow << getVal("BloodChemistry_Hemaocrit") << ",";
    labRow << getVal("CompleteBloodCount_Platelet") << ",";

    // CMP
    labRow << "CMP,";
    labRow << getVal("Substance_Albumin_Concentration") << ",";
    labRow << ","; // ALP
    labRow << ","; // ALT
    labRow << ","; // AST
    labRow << getVal("BloodChemistry_BloodUreaNitrogen_Concentration") << ",";
    labRow << getVal("Substance_Calcium_Concentration") << ",";
    labRow << getVal("MetabolicPanel_Chloride") << ",";
    labRow << getVal("MetabolicPanel_CarbonDioxide") << ",";
    labRow << getVal("Substance_Creatinine_Concentration") << ",";
    labRow << getVal("Substance_Creatinine_Concentration") << ",";
    labRow << getVal("Substance_Glucose_Concentration") << ",";
    labRow << getVal("MetabolicPanel_Potassium") << ",";
    labRow << getVal("Substance_Sodium") << ",";
    labRow << getVal("MetabolicPanel_Bilirubin") << ",";
    labRow << getVal("MetabolicPanel_Protein");

    {
        const std::lock_guard<std::mutex> lock(labs_mutex);
        labsStorage.push_back(labRow.str());
    }
}

std::string ExtractManikinIDFromString(std::string in)
{
    std::size_t pos = in.find("mid=");
    if (pos != std::string::npos)
    {
        std::string mid1 = in.substr(pos + 4);
        std::size_t pos1 = mid1.find(";");
        if (pos1 != std::string::npos)
        {
            std::string mid2 = mid1.substr(0, pos1);
            return mid2;
        }
        return mid1;
    }
    return {};
}

/// Core logic container for DDS Manager functions.
class RESTListener : public ListenerInterface
{
public:
    void onNewStatus(AMM::Status &st, SampleInfo_t *info)
    {
        ostringstream statusValue;
        statusValue << AMM::Utility::EStatusValueStr(st.value());

        LOG_DEBUG << "[" << st.module_id().id() << "][" << st.module_name() << "]["
                  << st.capability() << "] Status = " << statusValue.str() << " (" << st.value() << ")";
        // Message = " << st.message();

        if (st.module_name() == "AMM_FluidManager" || st.module_name() == "Torso_Control")
        {
            if (st.capability() == "fluidics")
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["FLUIDICS_STATE"] = statusValue.str();
            }
            else if (st.capability() == "clear_supply")
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["CLEAR_SUPPLY"] = statusValue.str();
            }
            else if (st.capability() == "blood_supply")
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["BLOOD_SUPPLY"] = statusValue.str();
            }
            else if (st.capability() == "air_supply")
            {
                {
                    const std::lock_guard<std::mutex> lock(status_mutex);
                    statusStorage["AIR_SUPPLY"] = statusValue.str();
                }
                // parse st.message() to double p; [p] = psi
                try
                {
                    double p = std::stod(st.message());
                    const std::lock_guard<std::mutex> lock(nds_mutex);
                    nodeDataStorage["Air_Pressure"] = p;
                }
                catch (const std::invalid_argument &)
                {
                    const std::lock_guard<std::mutex> lock(nds_mutex);
                    nodeDataStorage["Air_Pressure"] = 0.0;
                }
                catch (const std::out_of_range &)
                {
                }
            }
        }

        if (st.module_name() == "AJAMS_Services")
        {
            if (st.capability() == "battery-1")
            {
                {
                    const std::lock_guard<std::mutex> lock(status_mutex);
                    statusStorage["BATTERY1"] = statusValue.str();
                }
                // parse st.message() to double soc; [soc] = %
                try
                {
                    double soc = std::stod(st.message());
                    const std::lock_guard<std::mutex> lock(nds_mutex);
                    nodeDataStorage["Battery1_SOC"] = soc;
                }
                catch (const std::invalid_argument &)
                {
                    const std::lock_guard<std::mutex> lock(nds_mutex);
                    nodeDataStorage["Battery1_SOC"] = 0.0;
                }
                catch (const std::out_of_range &)
                {
                }
            }
            else if (st.capability() == "battery-2")
            {
                {
                    const std::lock_guard<std::mutex> lock(status_mutex);
                    statusStorage["BATTERY2"] = statusValue.str();
                }
                // parse st.message() to double soc; [soc] = %
                try
                {
                    double soc = std::stod(st.message());
                    const std::lock_guard<std::mutex> lock(nds_mutex);
                    nodeDataStorage["Battery2_SOC"] = soc;
                }
                catch (const std::invalid_argument &)
                {
                    const std::lock_guard<std::mutex> lock(nds_mutex);
                    nodeDataStorage["Battery2_SOC"] = 0.0;
                }
                catch (const std::out_of_range &)
                {
                }
            }
            else if (st.capability() == "ext_power")
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["EXT_POWER"] = statusValue.str();
            }
        }

        if (st.capability() == "iv_detection")
        {
            const std::lock_guard<std::mutex> lock(status_mutex);
            statusStorage["IVARM_STATE"] = statusValue.str();
        }
    }

    void onNewTick(AMM::Tick &t, SampleInfo_t *info)
    {
        const std::lock_guard<std::mutex> lock(status_mutex);
        if (statusStorage["STATUS"].compare("NOT RUNNING") == 0 &&
            t.frame() > lastTick)
        {
            statusStorage["STATUS"] = "RUNNING";
        }
        lastTick = t.frame();
        statusStorage["TICK"] = to_string(t.frame());
        statusStorage["TIME"] = to_string(t.time());
    }

    void onNewSimulationControl(AMM::SimulationControl &simControl, SampleInfo_t *info)
    {
        switch (simControl.type())
        {
        case AMM::ControlType::RUN:
        {
            const std::lock_guard<std::mutex> lock(status_mutex);
            statusStorage["STATUS"] = "RUNNING";
            LOG_DEBUG << "SimControl received: Run sim.";
            break;
        }

        case AMM::ControlType::HALT:
        {
            const std::lock_guard<std::mutex> lock(status_mutex);
            statusStorage["STATUS"] = "PAUSED";
            break;
        }

        case AMM::ControlType::RESET:
        {
            LOG_DEBUG
                << "SimControl received: Reset simulation, clean up and prepare for next run.";
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["STATUS"] = "NOT RUNNING";
                statusStorage["TICK"] = "0";
                statusStorage["TIME"] = "0";
                lastTick = 0;
            }
            {
                const std::lock_guard<std::mutex> lock(nds_mutex);
                nodeDataStorage.clear();
            }
            ResetLabs();
            break;
        }
        }
    }

    void onNewModuleConfiguration(AMM::ModuleConfiguration &mc, SampleInfo_t *info)
    {
        if (mc.name() == "physiology_engine")
        {
            std::string capabilities = mc.capabilities_configuration().to_string();
            std::size_t namePos = capabilities.find("name=\"state_file\"");
            if (namePos == std::string::npos)
            {
                namePos = capabilities.find("name='state_file'");
            }
            if (namePos != std::string::npos)
            {
                std::size_t tagStart = capabilities.rfind('<', namePos);
                std::size_t tagEnd = capabilities.find('>', namePos);
                if (tagStart != std::string::npos && tagEnd != std::string::npos && tagEnd > tagStart)
                {
                    std::string tag = capabilities.substr(tagStart, tagEnd - tagStart + 1);
                    std::size_t valPos = tag.find("value=");
                    if (valPos != std::string::npos && valPos + 6 < tag.size())
                    {
                        char quote = tag[valPos + 6];
                        if (quote == '"' || quote == '\'')
                        {
                            std::size_t endQuote = tag.find(quote, valPos + 7);
                            if (endQuote != std::string::npos)
                            {
                                std::string stateFile = tag.substr(valPos + 7, endQuote - (valPos + 7));
                                if (!stateFile.empty())
                                {
                                    LOG_INFO << "ModuleConfiguration physiology_engine state_file: " << stateFile;
                                    const std::lock_guard<std::mutex> lock(status_mutex);
                                    statusStorage["STATE"] = stateFile;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void onNewCommand(AMM::Command &c, SampleInfo_t *info)
    {
        std::string manikin_id = ExtractManikinIDFromString(c.message());
        LOG_INFO << "Got a command: " << c.message() << " for manikin " << manikin_id;
        if (!c.message().compare(0, sysPrefix.size(), sysPrefix))
        {
            std::string value = c.message().substr(sysPrefix.size());
            if (value.find("START_SIM") != std::string::npos)
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["STATUS"] = "RUNNING";
            }
            else if (value.find("STOP_SIM") != std::string::npos)
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["STATUS"] = "STOPPED";
            }
            else if (value.find("PAUSE_SIM") != std::string::npos)
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["STATUS"] = "PAUSED";
            }
            else if (value.find("RESET_SIM") != std::string::npos)
            {
                {
                    const std::lock_guard<std::mutex> lock(status_mutex);
                    statusStorage["STATUS"] = "NOT RUNNING";
                    statusStorage["TICK"] = "0";
                    statusStorage["TIME"] = "0";
                    lastTick = 0;
                }
                {
                    const std::lock_guard<std::mutex> lock(nds_mutex);
                    nodeDataStorage.clear();
                }
                ResetLabs();
            }
            else if (value.find("END_SIMULATION") != std::string::npos)
            {
                {
                    const std::lock_guard<std::mutex> lock(status_mutex);
                    statusStorage["STATUS"] = "STOPPED";
                    statusStorage["TICK"] = "0";
                    statusStorage["TIME"] = "0";
                    lastTick = 0;
                }
                {
                    const std::lock_guard<std::mutex> lock(nds_mutex);
                    nodeDataStorage.clear();
                }
                ResetLabs();
            }
            else if (value.find("APPEND_LABS") != std::string::npos)
            {
                AppendLabRow();
            }
            else if (value.find("CLEAR_LOG") != std::string::npos)
            {
            }
            else if (value.find("RESTART_SERVICE") != std::string::npos)
            {
                LOG_INFO << "Command: RESTART_SERVICE" << c.message();
            }
            else if (!value.compare(0, loadPrefix.size(), loadPrefix))
            {
                {
                    const std::lock_guard<std::mutex> lock(status_mutex);
                    statusStorage["STATE"] = value.substr(loadPrefix.size());
                }
                SendReset();
            }
            else if (!value.compare(0, loadScenarioPrefix.size(),
                                    loadScenarioPrefix))
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["SCENARIO"] = value.substr(loadScenarioPrefix.size());
            }
            else if (!value.compare(0, loadPatientPrefix.size(),
                                    loadPatientPrefix))
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["PATIENT"] = value.substr(loadPatientPrefix.size());
            }
        }
        else
        {
            // LOG_TRACE << "Unknown AMM Command: " << c.message();
        }
    }

    void onNewPhysiologyValue(AMM::PhysiologyValue &n, SampleInfo_t *info)
    {
      //      LOG_TRACE << "Getting physiology value: " << n.name() << " = " << n.value();
        const std::lock_guard<std::mutex> lock(nds_mutex);
        if (!isnan(n.value()))
        {
            nodeDataStorage[n.name()] = n.value();
        }
    }

    void onNewRenderModification(AMM::RenderModification &rendMod, SampleInfo_t *info)
    {
        std::ostringstream messageOut;
        messageOut << "[AMM_Render_Modification]"
                   << "type=" << rendMod.type() << ";"
                   << "payload=" << rendMod.data();
        std::string stringOut = messageOut.str();
        // LOG_DEBUG << "Render modification received from AMM: " << stringOut;

        const std::lock_guard<std::mutex> lock(status_mutex);

        if (rendMod.type().compare("CONNECT_ECG") == 0)
        {
            statusStorage["MONITOR_ECG"] = "ON";
        }
        else if (rendMod.type().compare("DETACH_ECG") == 0)
        {
            statusStorage["MONITOR_ECG"] = "OFF";
        }
        else if (rendMod.type().compare("CONNECT_PULSE_OX") == 0)
        {
            statusStorage["MONITOR_PULSEOX"] = "ON";
        }
        else if (rendMod.type().compare("DETACH_PULSE_OX") == 0)
        {
            statusStorage["MONITOR_PULSEOX"] = "OFF";
        }
        else if (rendMod.type().compare("CONNECT_NIBP") == 0)
        {
            statusStorage["MONITOR_NIBP"] = "ON";
        }
        else if (rendMod.type().compare("DETACH_NIBP") == 0)
        {
            statusStorage["MONITOR_NIBP"] = "OFF";
        }
        else if (rendMod.type().compare("CONNECT_TEMP_PROBE") == 0)
        {
            statusStorage["MONITOR_TEMP"] = "ON";
        }
        else if (rendMod.type().compare("DETACH_TEMP_PROBE") == 0)
        {
            statusStorage["MONITOR_TEMP"] = "OFF";
        }
        else if (rendMod.type().compare("CONNECT_ART_LINE") == 0)
        {
            statusStorage["MONITOR_ARTLINE"] = "ON";
        }
        else if (rendMod.type().compare("DETACH_ART_LINE") == 0)
        {
            statusStorage["MONITOR_ARTLINE"] = "OFF";
        }
        else if (rendMod.type().compare("CONNECT_ETCO2") == 0)
        {
            statusStorage["MONITOR_ETCO2"] = "ON";
        }
        else if (rendMod.type().compare("DETACH_ETCO2") == 0)
        {
            statusStorage["MONITOR_ETCO2"] = "OFF";
        }
        else if (rendMod.type().compare("ATTACH_TO_PATIENT") == 0)
        {
            statusStorage["MONITOR_ECG"] = "ON";
            statusStorage["MONITOR_PULSEOX"] = "ON";
            statusStorage["MONITOR_NIBP"] = "ON";
            statusStorage["MONITOR_TEMP"] = "ON";
            statusStorage["MONITOR_ARTLINE"] = "ON";
            statusStorage["MONITOR_ETCO2"] = "ON";
        }
        else if (rendMod.type().compare("DETACH_FROM_PATIENT") == 0)
        {
            statusStorage["MONITOR_ECG"] = "OFF";
            statusStorage["MONITOR_PULSEOX"] = "OFF";
            statusStorage["MONITOR_NIBP"] = "OFF";
            statusStorage["MONITOR_TEMP"] = "OFF";
            statusStorage["MONITOR_ARTLINE"] = "OFF";
            statusStorage["MONITOR_ETCO2"] = "OFF";
        }
    }
};

const std::string moduleName = "AMM_REST_Adapter";
const std::string configFile = "config/rest_adapter_amm.xml";
DDSManager<RESTListener> *mgr;
AMM::UUID m_uuid;

database db("mohses.db");

void SendReset()
{
    AMM::SimulationControl simControl;
    auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    simControl.timestamp(ms);
    simControl.type(AMM::ControlType::RESET);
    mgr->WriteSimulationControl(simControl);
}

void PublishOperationalDescription()
{
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

AMM::UUID SendEventRecord(
    const std::string &location,
    const std::string &practitioner,
    const std::string &type)
{
    LOG_DEBUG << "Publishing an event record: " << type;
    AMM::EventRecord er;
    AMM::FMA_Location fmaL;
    fmaL.name(location);

    AMM::UUID eventUUID;
    eventUUID.id(mgr->GenerateUuidString());

    auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    er.timestamp(ms);
    er.type(type);
    er.location(fmaL);
    er.id(eventUUID);
    if (!practitioner.empty())
    {
        AMM::UUID agentId;
        agentId.id(practitioner);
        er.agent_id(agentId);
    }
    mgr->WriteEventRecord(er);
    return eventUUID;
}

void SendPhysiologyModification(
    AMM::UUID &er_id,
    const std::string &type,
    const std::string &payload)
{
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
                            const std::string &payload)
{
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
    const std::string &comment)
{
    LOG_INFO << "Publishing an assessment: " << assessment_type;
    AMM::Assessment assessInstance;
    AMM::UUID instUUID;
    instUUID.id(mgr->GenerateUuidString());
    assessInstance.id(instUUID);
    assessInstance.event_id(er_id);
    if (assessment_type == "SUCCESS")
    {
        assessInstance.value(AMM::AssessmentValue::SUCCESS);
    }
    else if (assessment_type == "COMMISSION_ERROR")
    {
        assessInstance.value(AMM::AssessmentValue::COMMISSION_ERROR);
    }
    else if (assessment_type == "EXECUTION_ERROR")
    {
        assessInstance.value(AMM::AssessmentValue::EXECUTION_ERROR);
    }
    else if (assessment_type == "OMISSION_ERROR")
    {
        assessInstance.value(AMM::AssessmentValue::OMISSION_ERROR);
    }
    std::string fullComment = comment;
    if (!step.empty())
    {
        fullComment = fullComment.empty() ? ("step=" + step) : (fullComment + " [step=" + step + "]");
    }
    if (!assessment_info.empty())
    {
        fullComment = fullComment.empty() ? assessment_info : (fullComment + " (" + assessment_info + ")");
    }
    assessInstance.comment(fullComment);
    mgr->WriteAssessment(assessInstance);
}

void SendCommand(const std::string &command)
{
    LOG_INFO << "Publishing a command:" << command;
    if (!command.compare(0, sysPrefix.size(), sysPrefix))
    {
        std::string value = command.substr(sysPrefix.size());
        if (value.compare("START_SIM") == 0)
        {
            AMM::SimulationControl simControl;
            auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            simControl.timestamp(ms);
            simControl.type(AMM::ControlType::RUN);
            mgr->WriteSimulationControl(simControl);
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["STATUS"] = "RUNNING";
            }
        }
        else if (value.compare("STOP_SIM") == 0)
        {
            AMM::SimulationControl simControl;
            auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            simControl.timestamp(ms);
            simControl.type(AMM::ControlType::HALT);
            mgr->WriteSimulationControl(simControl);
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["STATUS"] = "STOPPED";
            }
        }
        else if (value.compare("PAUSE_SIM") == 0)
        {
            AMM::SimulationControl simControl;
            auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            simControl.timestamp(ms);
            simControl.type(AMM::ControlType::HALT);
            mgr->WriteSimulationControl(simControl);
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["STATUS"] = "PAUSED";
            }
        }
        else if (value.compare("RESET_SIM") == 0)
        {
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["STATUS"] = "NOT RUNNING";
                statusStorage["TICK"] = "0";
                statusStorage["TIME"] = "0";
                lastTick = 0;
            }
            {
                const std::lock_guard<std::mutex> lock(nds_mutex);
                nodeDataStorage.clear();
            }
            ResetLabs();
            AMM::SimulationControl simControl;
            auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            simControl.timestamp(ms);
            simControl.type(AMM::ControlType::RESET);
            mgr->WriteSimulationControl(simControl);
        }
        else if (value.compare("SAVE_STATE") == 0)
        {
            AMM::SimulationControl simControl;
            auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            simControl.timestamp(ms);
            simControl.type(AMM::ControlType::SAVE);
            mgr->WriteSimulationControl(simControl);
        }
        else if (value.compare("RESTART_SERVICES") == 0)
        {
            // Execute restart request here. Do not forward.
            // TODO: this functionality might be more appropriate for the module manager to handle
            //       the else statement below will forward. Module manager would receive the command and process
            std::string command = "supervisorctl start amm_startup";
            int result = boost::process::system(command);
            if (result != 0) {
                LOG_WARNING << "supervisorctl command failed with code: " << result;
            }
        }
        else
        {
            if (!value.compare(0, loadPrefix.size(), loadPrefix))
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["STATE"] = value.substr(loadPrefix.size());
            }
            else if (!value.compare(0, loadScenarioPrefix.size(), loadScenarioPrefix))
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["SCENARIO"] = value.substr(loadScenarioPrefix.size());
            }
            else if (!value.compare(0, loadPatientPrefix.size(), loadPatientPrefix))
            {
                const std::lock_guard<std::mutex> lock(status_mutex);
                statusStorage["PATIENT"] = value.substr(loadPatientPrefix.size());
            }
            // Publish a SYS Command
            AMM::Command cmdInstance;
            cmdInstance.message(command);
            mgr->WriteCommand(cmdInstance);
        }
    }
    else
    {
        // Publish some other command?
        AMM::Command cmdInstance;
        cmdInstance.message(command);
        mgr->WriteCommand(cmdInstance);
    }
}

void printCookies(const Http::Request &req)
{
    auto cookies = req.cookies();
    std::cout << "Cookies: [" << std::endl;
    const std::string indent(4, ' ');
    for (const auto &c : cookies)
    {
        std::cout << indent << c.name << " = " << c.value << std::endl;
    }
    std::cout << "]" << std::endl;
}

namespace Generic
{

    void handleReady(const Rest::Request &request, Http::ResponseWriter response)
    {
        response.send(Http::Code::Ok, "1");
    }
}

class DDSEndpoint
{
    static constexpr size_t DefaultMaxPayload = 1024102410;

public:
    explicit DDSEndpoint(Address addr)
        : httpEndpoint(std::make_shared<Http::Endpoint>(addr)) {}

    void init(int thr = 2)
    {
        auto opts = Http::Endpoint::options()
                        .threads(thr)
                        .flags(Tcp::Options::ReuseAddr)
                        .maxRequestSize(DefaultMaxPayload);
        httpEndpoint->init(opts);
        setupRoutes();
    }

    void start()
    {
        httpEndpoint->setHandler(router.handler());
        httpEndpoint->serveThreaded();
    }

    void shutdown() { httpEndpoint->shutdown(); }

private:
    void setupRoutes()
    {
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
        Routes::Get(router, "/scenario/:name",
                    Routes::bind(&DDSEndpoint::getScenario, this));

        Routes::Get(router, "/states", Routes::bind(&DDSEndpoint::getStates, this));
        Routes::Get(router, "/states/:name/delete",
                    Routes::bind(&DDSEndpoint::deleteState, this));
    }

    void getInstance(const Rest::Request &request,
                     Http::ResponseWriter response)
    {
        StringBuffer s;
        Writer<StringBuffer> writer(s);

        std::string scenario;
        {
            const std::lock_guard<std::mutex> lock(status_mutex);
            scenario = statusStorage["SCENARIO"];
        }
        if (scenario.empty()) {
            std::ifstream t("static/current_scenario.txt");
            if (t.is_open()) {
                scenario = std::string((std::istreambuf_iterator<char>(t)),
                                       std::istreambuf_iterator<char>());
                t.close();
            }
        }

        writer.StartObject();
        writer.Key("name");
        writer.String(hostname);
        writer.Key("scenario");
        writer.String(scenario.c_str());
        writer.EndObject();

        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getStates(const Rest::Request &request, Http::ResponseWriter response)
    {
        StringBuffer s;
        Writer<StringBuffer> writer(s);

        writer.StartArray();
        if (exists(state_path) && is_directory(state_path))
        {
            std::vector<boost::filesystem::path> paths(
                boost::filesystem::directory_iterator{state_path}, boost::filesystem::directory_iterator{});
            std::sort(paths.begin(), paths.end());
            for (auto const &path : paths)
            {
                if (is_regular_file(path))
                {
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
                     Http::ResponseWriter response)
    {
        auto name = urlDecode(request.param(":name").as<std::string>());
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");

        if (!isValidFilename(name))
        {
            LOG_WARNING << "Rejected invalid state filename request: " << name;
            response.send(Pistache::Http::Code::Bad_Request,
                          "Invalid state filename", MIME(Application, Json));
            return;
        }

        if (name != "StandardMale@0s.xml")
        {
            path deletePath = path(state_path) / path(name).filename();
            if (exists(deletePath) && is_regular_file(deletePath))
            {
                LOG_INFO << "Deleting " << deletePath;
                boost::filesystem::remove(deletePath);
                response.send(Pistache::Http::Code::Ok, "Deleted",
                              MIME(Application, Json));
            }
            else
            {
                response.send(Pistache::Http::Code::Forbidden,
                              "Unable to delete state file", MIME(Application, Json));
            }
        }
        else
        {
            response.send(Pistache::Http::Code::Forbidden,
                          "Can not delete default state file",
                          MIME(Application, Json));
        }
    }

    void getScenarios(const Rest::Request &request,
                      Http::ResponseWriter response)
    {
        StringBuffer s;
        Writer<StringBuffer> writer(s);

        writer.StartArray();
        if (exists(scenario_path) && is_directory(scenario_path))
        {
            // Create a vector to hold all paths for sorting
            std::vector<boost::filesystem::path> paths(
                boost::filesystem::directory_iterator{scenario_path}, boost::filesystem::directory_iterator{});
            // Sort the paths alphabetically
            std::sort(paths.begin(), paths.end());

            // Now iterate through the sorted paths
            for (auto const &path : paths)
            {
                if (is_regular_file(path))
                {
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
        response.send(Pistache::Http::Code::Ok, s.GetString(),
                      MIME(Application, Json));
    }

    void getScenario(const Rest::Request &request,
                     Http::ResponseWriter response)
    {
        std::string name = urlDecode(request.param(":name").as<std::string>());
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");

        if (!isValidFilename(name))
        {
            LOG_WARNING << "Rejected invalid scenario filename request: " << name;
            response.send(Pistache::Http::Code::Bad_Request, "Invalid scenario filename");
            return;
        }

        try
        {
            path scenarioFile = path(scenario_path) / path(name).filename();

            if (!exists(scenarioFile) || !is_regular_file(scenarioFile))
            {
                response.send(Pistache::Http::Code::Not_Found, "Scenario file does not exist");
                return;
            }

            LOG_INFO << "Received a request to GET a scenario file, so we're serving this up: " << scenarioFile.string();
            Http::serveFile(response, scenarioFile.string().c_str(), MIME(Application, Xml));
        }
        catch (const filesystem_error &e)
        {
            LOG_ERROR << "Filesystem error serving scenario file: " << e.what();
            response.send(Pistache::Http::Code::Not_Found, "Scenario file does not exist");
        }
    }

    void getPatients(const Rest::Request &request,
                     Http::ResponseWriter response)
    {
        StringBuffer s;
        Writer<StringBuffer> writer(s);

        writer.StartArray();
        if (exists(patient_path) && is_directory(patient_path))
        {
            std::vector<boost::filesystem::path> paths(
                boost::filesystem::directory_iterator{patient_path}, boost::filesystem::directory_iterator{});
            std::sort(paths.begin(), paths.end());
            for (auto const &path : paths)
            {
                if (is_regular_file(path))
                {
                    writer.StartObject();
                    writer.Key("name");
                    writer.String(path.filename().c_str());
                    writer.Key("description");
                    stringstream writeTime;
                    writeTime << last_write_time(path);
                    writer.String(writeTime.str().c_str());
                    writer.EndObject();
                }
            }
        }
        writer.EndArray();

        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Pistache::Http::Code::Ok, s.GetString(),
                      MIME(Application, Json));
    }

    void getAssessments(const Rest::Request &request, Http::ResponseWriter response)
    {
        LOG_INFO << "Get a list of all assessment CSVs";
        StringBuffer s;
        Writer<StringBuffer> writer(s);

        writer.StartArray();
        if (exists(assessment_path) && is_directory(assessment_path))
        {
            std::vector<boost::filesystem::path> paths(
                boost::filesystem::directory_iterator{assessment_path}, boost::filesystem::directory_iterator{});
            std::sort(paths.begin(), paths.end());
            for (auto const &p : paths)
            {
                if (is_regular_file(p))
                {
                    writer.StartObject();
                    writer.Key("name");
                    writer.String(p.filename().c_str());
                    writer.Key("last_updated");
                    stringstream writeTime;
                    writeTime << last_write_time(p);
                    writer.String(writeTime.str().c_str());
                    writer.EndObject();
                }
            }
        }
        writer.EndArray();

        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Pistache::Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    int writeToFile(const string &filename, const string &data)
    {
        try
        {
            path filePath(filename);
            if (filePath.has_parent_path() && !exists(filePath.parent_path()))
            {
                create_directories(filePath.parent_path());
            }
        }
        catch (const filesystem_error &e)
        {
            LOG_ERROR << "Failed to create parent directory for " << filename << ": " << e.what();
            return -1;
        }

        LOG_INFO << "Writing to " << filename;
        std::ofstream out(filename, std::ios::out | std::ios::trunc);
        if (!out.is_open() || out.fail())
        {
            LOG_ERROR << "Open failure: " << strerror(errno);
            return -1;
        }

        out << data;
        out.close();

        if (out.fail())
        {
            LOG_ERROR << "Write failure: " << strerror(errno);
            return -1;
        }

        LOG_INFO << "All done";
        return 0;
    }

    void createAssessment(const Rest::Request &request,
                          Http::ResponseWriter response)
    {
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        auto name = urlDecode(request.param(":name").as<std::string>());
        if (name.empty())
        {
            LOG_INFO << "Name was empty";
            name = "test.csv";
        }

        if (!isValidFilename(name))
        {
            LOG_WARNING << "Rejected invalid assessment filename request: " << name;
            response.send(Pistache::Http::Code::Bad_Request, "Invalid assessment filename");
            return;
        }

        path assessmentFile = path(assessment_path) / path(name).filename();
        LOG_INFO << "Create an assessment from a POST.  Filename is " << assessmentFile.string();
        auto s = std::chrono::steady_clock::now();
        int rc = writeToFile(assessmentFile.string(), request.body());
        auto dt = std::chrono::steady_clock::now() - s;
        LOG_INFO << "File upload processed in: " << std::chrono::duration_cast<std::chrono::microseconds>(dt).count();

        if (rc != 0)
        {
            response.send(Http::Code::Internal_Server_Error, "Failed to write assessment file");
            return;
        }

        response.send(Http::Code::Ok);

        LOG_INFO << "Sending out system message that there's an assessment available.";
        std::string command = "[SYS]ASSESSMENT_AVAILABLE:" + name;
        AMM::Command cmdInstance;
        cmdInstance.message(command);
        mgr->WriteCommand(cmdInstance);
    }

    void deleteAssessment(const Rest::Request &request,
                          Http::ResponseWriter response)
    {
        auto name = urlDecode(request.param(":name").as<std::string>());
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");

        if (!isValidFilename(name))
        {
            LOG_WARNING << "Rejected invalid assessment filename request: " << name;
            response.send(Pistache::Http::Code::Bad_Request, "Invalid assessment filename");
            return;
        }

        path deletePath = path(assessment_path) / path(name).filename();
        if (exists(deletePath) && is_regular_file(deletePath))
        {
            LOG_INFO << "Deleting assessment " << deletePath;
            boost::filesystem::remove(deletePath);
            response.send(Pistache::Http::Code::Ok, "Deleted", MIME(Application, Json));
        }
        else
        {
            response.send(Pistache::Http::Code::Not_Found, "Assessment file does not exist");
        }
    }

    void getAssessment(const Rest::Request &request, Http::ResponseWriter response)
    {
        std::string name = urlDecode(request.param(":name").as<std::string>());
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");

        if (!isValidFilename(name))
        {
            LOG_WARNING << "Rejected invalid assessment filename request: " << name;
            response.send(Pistache::Http::Code::Bad_Request, "Invalid assessment filename");
            return;
        }

        path assessmentFile = path(assessment_path) / path(name).filename();
        if (!exists(assessmentFile) || !is_regular_file(assessmentFile))
        {
            response.send(Pistache::Http::Code::Not_Found, "Assessment file does not exist");
            return;
        }

        LOG_INFO << "Received a request to GET an assessment file, so we're serving this up: " << assessmentFile.string();
        Http::serveFile(response, assessmentFile.string().c_str());
    }

    void getActions(const Rest::Request &request, Http::ResponseWriter response)
    {
        StringBuffer s;
        Writer<StringBuffer> writer(s);

        writer.StartArray();
        if (exists(action_path) && is_directory(action_path))
        {
            std::vector<boost::filesystem::path> paths(
                boost::filesystem::directory_iterator{action_path}, boost::filesystem::directory_iterator{});
            std::sort(paths.begin(), paths.end());
            for (auto const &p : paths)
            {
                if (is_regular_file(p))
                {
                    writer.StartObject();
                    writer.Key("name");
                    writer.String(p.filename().c_str());
                    writer.Key("description");
                    stringstream writeTime;
                    writeTime << last_write_time(p);
                    writer.String(writeTime.str().c_str());
                    writer.EndObject();
                }
            }
        }
        writer.EndArray();

        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Pistache::Http::Code::Ok, s.GetString(),
                      MIME(Application, Json));
    }

    void executeCommand(const Rest::Request &request,
                        Http::ResponseWriter response)
    {
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.headers().add<Http::Header::AccessControlAllowHeaders>("*");
        try {
            Document document;
            document.Parse(request.body().c_str());
            if (document.HasParseError() || !document.IsObject() ||
                !document.HasMember("payload") || !document["payload"].IsString()) {
                response.send(Pistache::Http::Code::Bad_Request,
                             "{\"error\":\"Invalid JSON or missing string payload\"}",
                             MIME(Application, Json));
                return;
            }
            std::string payload = document["payload"].GetString();

            SendCommand(payload);

            response.send(Pistache::Http::Code::Ok,
                          "{\"message\":\"Command executed\"}",
                          MIME(Application, Json));
        } catch (const std::exception& e) {
            LOG_ERROR << "JSON parsing error in executeCommand: " << e.what();
            response.send(Pistache::Http::Code::Bad_Request,
                         "{\"error\":\"JSON parsing failed\"}",
                         MIME(Application, Json));
        }
    }

    void executePhysiologyModification(const Rest::Request &request,
                                       Http::ResponseWriter response)
    {
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.headers().add<Http::Header::AccessControlAllowHeaders>("*");
        try {
            Document document;
            document.Parse(request.body().c_str());
            if (document.HasParseError() || !document.IsObject()) {
                response.send(Pistache::Http::Code::Bad_Request,
                             "{\"error\":\"Invalid JSON\"}",
                             MIME(Application, Json));
                return;
            }
            std::string type = getJsonString(document, "type");
            std::string location = getJsonString(document, "location");
            std::string practitioner = getJsonString(document, "practitioner");
            std::string payload = getJsonString(document, "payload");

            AMM::UUID erID = SendEventRecord(location, practitioner, type);
            SendPhysiologyModification(erID, type, payload);
            response.send(Pistache::Http::Code::Ok,
                          "{\"message\":\"Physiology modification published\"}",
                          MIME(Application, Json));
        } catch (const std::exception& e) {
            LOG_ERROR << "JSON parsing error in executePhysiologyModification: " << e.what();
            response.send(Pistache::Http::Code::Bad_Request,
                         "{\"error\":\"JSON parsing failed\"}",
                         MIME(Application, Json));
        }
    }

    void executeRenderModification(const Rest::Request &request,
                                   Http::ResponseWriter response)
    {
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.headers().add<Http::Header::AccessControlAllowHeaders>("*");
        try {
            Document document;
            document.Parse(request.body().c_str());
            if (document.HasParseError() || !document.IsObject()) {
                response.send(Pistache::Http::Code::Bad_Request,
                             "{\"error\":\"Invalid JSON\"}",
                             MIME(Application, Json));
                return;
            }
            std::string type = getJsonString(document, "type");
            std::string location = getJsonString(document, "location");
            std::string practitioner = getJsonString(document, "practitioner");
            std::string payload = getJsonString(document, "payload");

            AMM::UUID erID = SendEventRecord(location, practitioner, type);
            SendRenderModification(erID, type, payload);
            response.send(Pistache::Http::Code::Ok,
                          "{\"message\":\"Render modification published\"}",
                          MIME(Application, Json));
        } catch (const std::exception& e) {
            LOG_ERROR << "JSON parsing error in executeRenderModification: " << e.what();
            response.send(Pistache::Http::Code::Bad_Request,
                         "{\"error\":\"JSON parsing failed\"}",
                         MIME(Application, Json));
        }
    }

    void executePerformanceAssessment(const Rest::Request &request,
                                      Http::ResponseWriter response)
    {
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.headers().add<Http::Header::AccessControlAllowHeaders>("*");
        try {
            Document document;
            document.Parse(request.body().c_str());
            if (document.HasParseError() || !document.IsObject()) {
                response.send(Pistache::Http::Code::Bad_Request,
                             "{\"error\":\"Invalid JSON\"}",
                             MIME(Application, Json));
                return;
            }
            std::string type = getJsonString(document, "type");
            if (type.empty()) {
                type = getJsonString(document, "assessment_type");
            }
            std::string location = getJsonString(document, "location");
            std::string practitioner = getJsonString(document, "practitioner");
            std::string info = getJsonString(document, "info");
            if (info.empty()) {
                info = getJsonString(document, "assessment_info");
            }
            std::string step = getJsonString(document, "step");
            std::string comment = getJsonString(document, "comment");

            AMM::UUID erID = SendEventRecord(location, practitioner, type);
            SendPerformanceAssessment(erID, type, info, step, comment);
            response.send(Pistache::Http::Code::Ok,
                          "{\"message\":\"Performance assessment published\"}",
                          MIME(Application, Json));
        } catch (const std::exception& e) {
            LOG_ERROR << "JSON parsing error in executePerformanceAssessment: " << e.what();
            response.send(Pistache::Http::Code::Bad_Request,
                         "{\"error\":\"JSON parsing failed\"}",
                         MIME(Application, Json));
        }
    }

    void executeOptions(const Rest::Request &request,
                        Http::ResponseWriter response)
    {
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.headers().add<Http::Header::AccessControlAllowHeaders>("*");
        response.send(Pistache::Http::Code::Ok, "{\"message\":\"success\"}", MIME(Application, Json));
    }

    void createAction(const Rest::Request &request,
                      Http::ResponseWriter response)
    {
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Pistache::Http::Code::Not_Implemented,
                      "{\"error\":\"createAction not implemented\"}",
                      MIME(Application, Json));
    }

    void deleteAction(const Rest::Request &request,
                      Http::ResponseWriter response)
    {
        auto name = urlDecode(request.param(":name").as<std::string>());
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");

        if (!isValidFilename(name))
        {
            response.send(Pistache::Http::Code::Bad_Request, "Invalid action filename");
            return;
        }

        path deletePath = path(action_path) / path(name).filename();
        if (exists(deletePath) && is_regular_file(deletePath))
        {
            LOG_INFO << "Deleting action " << deletePath;
            boost::filesystem::remove(deletePath);
            response.send(Pistache::Http::Code::Ok, "Deleted", MIME(Application, Json));
        }
        else
        {
            response.send(Pistache::Http::Code::Not_Found, "Action file does not exist");
        }
    }

    void updateAction(const Rest::Request &request,
                      Http::ResponseWriter response)
    {
        auto name = urlDecode(request.param(":name").as<std::string>());
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");

        if (!isValidFilename(name))
        {
            response.send(Pistache::Http::Code::Bad_Request, "Invalid action filename");
            return;
        }

        path actionFile = path(action_path) / path(name).filename();
        if (writeToFile(actionFile.string(), request.body()) != 0)
        {
            response.send(Pistache::Http::Code::Internal_Server_Error, "Failed to write action file");
            return;
        }
        response.send(Pistache::Http::Code::Ok, "{\"message\":\"Action updated\"}", MIME(Application, Json));
    }

    void getAction(const Rest::Request &request, Http::ResponseWriter response)
    {
        auto name = urlDecode(request.param(":name").as<std::string>());
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");

        if (!isValidFilename(name))
        {
            response.send(Pistache::Http::Code::Bad_Request, "Invalid action filename");
            return;
        }

        path actionFile = path(action_path) / path(name).filename();
        if (!exists(actionFile) || !is_regular_file(actionFile))
        {
            response.send(Pistache::Http::Code::Not_Found, "Action file does not exist");
            return;
        }
        Http::serveFile(response, actionFile.string().c_str());
    }

    void issueCommand(const Rest::Request &request,
                      Http::ResponseWriter response)
    {
        auto name = urlDecode(request.param(":name").as<std::string>());
        SendCommand(name);
        StringBuffer s;
        Writer<StringBuffer> writer(s);
        writer.StartObject();
        writer.Key("Sent command");
        writer.String(name.c_str());
        writer.EndObject();
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getModuleById(const Rest::Request &request,
                       Http::ResponseWriter response)
    {
        auto id = request.param(":id").as<std::string>();
        StringBuffer s;
        Writer<StringBuffer> writer(s);

        try
        {
            const std::lock_guard<std::mutex> lock(db_mutex);
            db << "SELECT "
                  "module_id AS module_id,"
                  "module_guid as module_guid,"
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
                    string capabilities, string manufacturer, string model)
            {
                writer.StartObject();

                writer.Key("Module_ID");
                writer.String(module_id.c_str());

                writer.Key("Module_GUID");
                writer.String(module_guid.c_str());

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
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Database error in getModuleById: " << e.what();
        }

        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getModuleByGuid(const Rest::Request &request,
                         Http::ResponseWriter response)
    {
        auto guid = request.param(":guid").as<std::string>();
        StringBuffer s;
        Writer<StringBuffer> writer(s);

        try
        {
            const std::lock_guard<std::mutex> lock(db_mutex);
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
                    string capabilities, string manufacturer, string model)
            {
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
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Database error in getModuleByGuid: " << e.what();
        }

        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getModuleCount(const Rest::Request &request,
                        Http::ResponseWriter response)
    {
        StringBuffer s;
        Writer<StringBuffer> writer(s);

        int totalCount = 0;
        int coreCount = 0;
        int otherCount = 0;

        try
        {
            const std::lock_guard<std::mutex> lock(db_mutex);
            db << "SELECT COUNT(DISTINCT module_name) FROM module_capabilities" >> totalCount;
            db << "SELECT COUNT(DISTINCT module_name) FROM module_capabilities where module_name LIKE 'AMM_%'" >> coreCount;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Database error in getModuleCount: " << e.what();
        }

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

    void getOtherModules(const Rest::Request &request, Http::ResponseWriter response)
    {
        StringBuffer s;
        Writer<StringBuffer> writer(s);
        writer.StartArray();

        try
        {
            const std::lock_guard<std::mutex> lock(db_mutex);
            db << "SELECT DISTINCT module_name FROM module_capabilities where module_name NOT LIKE 'AMM_%'" >> [&](string module_name)
            {
                writer.String(module_name.c_str());
            };
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Database error in getOtherModules: " << e.what();
        }

        writer.EndArray();
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getModules(const Rest::Request &request, Http::ResponseWriter response)
    {
        StringBuffer s;
        Writer<StringBuffer> writer(s);
        writer.StartArray();

        try
        {
            const std::lock_guard<std::mutex> lock(db_mutex);
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
                    string manufacturer, string model)
            {
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
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Database error in getModules: " << e.what();
        }

        writer.EndArray();

        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getEventLog(const Rest::Request &request,
                     Http::ResponseWriter response)
    {
        StringBuffer s;
        Writer<StringBuffer> writer(s);
        writer.StartArray();

        try
        {
            const std::lock_guard<std::mutex> lock(db_mutex);
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
                [&](string module_id, string module_name, string source, string topic, int64_t timestamp, string data)
            {
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
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Database error in getEventLog: " << e.what();
        }

        writer.EndArray();
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getEventLogCSV(const Rest::Request &request,
                        Http::ResponseWriter response)
    {
        std::ostringstream s;

        try
        {
            const std::lock_guard<std::mutex> lock(db_mutex);
            db << "SELECT "
                  "module_capabilities.module_name,"
                  "events.source,"
                  "events.topic,"
                  "events.timestamp,"
                  "events.data "
                  "FROM "
                  "events "
                  "LEFT JOIN module_capabilities "
                  "ON "
                  "events.source = module_capabilities.module_guid" >>
                [&](string module_name, string source, string topic, int64_t timestamp,
                    string data)
            {
                s << formatTimestampCsv(timestamp) << ","
                  << escapeCsvField(module_name) << ","
                  << escapeCsvField(source) << ","
                  << escapeCsvField(topic) << ","
                  << escapeCsvField(data) << "\n";
            };
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Database error in getEventLogCSV: " << e.what();
        }

        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        auto csvHeader = Http::Header::Raw("Content-Disposition", "attachment;filename=amm_timeline_log.csv");
        response.headers().addRaw(csvHeader);
        response.send(Http::Code::Ok, s.str(), Http::Mime::MediaType::fromString("text/csv"));
    }

    void getDiagnosticLog(const Rest::Request &request,
                          Http::ResponseWriter response)
    {
        StringBuffer s;
        Writer<StringBuffer> writer(s);
        writer.StartArray();

        try
        {
            const std::lock_guard<std::mutex> lock(db_mutex);
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
                    int64_t timestamp)
            {
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
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Database error in getDiagnosticLog: " << e.what();
        }

        writer.EndArray();
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getDiagnosticLogCSV(const Rest::Request &request,
                             Http::ResponseWriter response)
    {
        std::ostringstream s;

        try
        {
            const std::lock_guard<std::mutex> lock(db_mutex);
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
                    int64_t timestamp)
            {
                s << formatTimestampCsv(timestamp) << ","
                  << escapeCsvField(log_level) << ","
                  << escapeCsvField(module_name) << ","
                  << escapeCsvField(message) << "\n";
            };
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Database error in getDiagnosticLogCSV: " << e.what();
        }

        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        auto csvHeader = Http::Header::Raw("Content-Disposition", "attachment;filename=amm_diagnostic_log.csv");
        response.headers().addRaw(csvHeader);
        response.send(Http::Code::Ok, s.str(), Http::Mime::MediaType::fromString("text/csv"));
    }

    void getNodes(const Rest::Request &request, Http::ResponseWriter response)
    {
        StringBuffer s;
        Writer<StringBuffer> writer(s);
        writer.StartArray();

        {
            const std::lock_guard<std::mutex> lock(nds_mutex);
            for (const auto& [key, value] : nodeDataStorage)
            {
                writer.StartObject();
                writer.Key(key.c_str());
                writer.Double(value);
                writer.EndObject();
            }
        }

        {
            const std::lock_guard<std::mutex> lock(status_mutex);
            for (const auto& [key, value] : statusStorage)
            {
                writer.StartObject();
                writer.Key(key.c_str());
                writer.String(value.c_str());
                writer.EndObject();
            }
        }

        writer.EndArray();
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
    }

    void getLabsReport(const Rest::Request &request, Http::ResponseWriter response)
    {
        std::string labReport;
        {
            const std::lock_guard<std::mutex> lock(labs_mutex);
            labReport = boost::algorithm::join(labsStorage, "\n");
        }
        auto mime = Http::Mime::MediaType::fromString("text/csv");
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok, labReport, mime);
    }

    void getNode(const Rest::Request &request, Http::ResponseWriter response)
    {
        auto name = urlDecode(request.param(":name").as<std::string>());
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");

        const std::lock_guard<std::mutex> lock(nds_mutex);
        auto it = nodeDataStorage.find(name);
        if (it != nodeDataStorage.end())
        {
            StringBuffer s;
            Writer<StringBuffer> writer(s);
            writer.StartObject();
            writer.Key(it->first.c_str());
            writer.Double(it->second);
            writer.EndObject();
            response.send(Http::Code::Ok, s.GetString(), MIME(Application, Json));
        }
        else
        {
            response.send(Http::Code::Not_Found, "Node data does not exist");
        }
    }

    void doDebug(const Rest::Request &request, Http::ResponseWriter response)
    {
        printCookies(request);
        response.cookies().add(Http::Cookie("lang", "en-US"));
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok);
    }

    void doShutdown(const Rest::Request &request, Http::ResponseWriter response)
    {
        m_runThread = false;
        shutdown_cv.notify_all();
        response.cookies().add(Http::Cookie("lang", "en-US"));
        response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        response.send(Http::Code::Ok);
    }

    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;
};

static void handleSignal(int signum)
{
    LOG_INFO << "Received signal " << signum << ", initiating shutdown.";
    m_runThread = false;
    shutdown_cv.notify_all();
}

static void show_usage(const std::string &name)
{
    cerr << "Usage: " << name << " <option(s)>"
         << "\nOptions:\n"
         << "\t-h,--help\t\tShow this help message\n"
         << endl;
}

Port port(static_cast<uint16_t>(portNumber));
Address addr(Ipv4::any(), port);
DDSEndpoint server(addr);

int main(int argc, char *argv[])
{
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::verbose, &consoleAppender);

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if ((arg == "-h") || (arg == "--help"))
        {
            show_usage(argv[0]);
            return 0;
        }

        if (arg == "-d")
        {
            daemonize = 1;
        }

        if (arg == "-nodiscovery")
        {
            discovery = 0;
        }
    }

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    {
        const std::lock_guard<std::mutex> lock(db_mutex);
        sqlite3_busy_timeout(db.connection().get(), 3000);
    }

    ResetLabs();

    RESTListener al;
    mgr = new AMM::DDSManager<RESTListener>(configFile);

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

    mgr->CreateTickSubscriber(&al, &RESTListener::onNewTick);
    mgr->CreatePhysiologyValueSubscriber(&al, &RESTListener::onNewPhysiologyValue);
    mgr->CreateCommandSubscriber(&al, &RESTListener::onNewCommand);
    mgr->CreateStatusSubscriber(&al, &RESTListener::onNewStatus);
    mgr->CreateRenderModificationSubscriber(&al, &RESTListener::onNewRenderModification);
    mgr->CreateSimulationControlSubscriber(&al, &RESTListener::onNewSimulationControl);
    mgr->CreateModuleConfigurationSubscriber(&al, &RESTListener::onNewModuleConfiguration);

    mgr->CreateAssessmentPublisher();
    mgr->CreateRenderModificationPublisher();
    mgr->CreatePhysiologyModificationPublisher();
    mgr->CreateSimulationControlPublisher();
    mgr->CreateCommandPublisher();
    mgr->CreateEventRecordPublisher();

    mgr->CreateOperationalDescriptionPublisher();
    mgr->CreateModuleConfigurationPublisher();
    mgr->CreateStatusPublisher();

    m_uuid.id(mgr->GenerateUuidString());

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    PublishOperationalDescription();

    gethostname(hostname, HOST_NAME_MAX);

    server.init(thr);
    LOG_INFO << "Listening on *:" << portNumber;

    m_runThread = true;

    server.start();

    LOG_INFO << "Ready.";

    std::thread consoleThread([]() {
        std::string action;
        while (m_runThread && std::getline(std::cin, action))
        {
            std::transform(action.begin(), action.end(), action.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            if (action == "EXIT")
            {
                LOG_INFO << "Shutting down from command-line.";
                m_runThread = false;
                shutdown_cv.notify_all();
                break;
            }
        }
    });
    consoleThread.detach();

    {
        std::unique_lock<std::mutex> lock(shutdown_mutex);
        shutdown_cv.wait(lock, []() { return !m_runThread.load(); });
    }

    server.shutdown();

    if (mgr != nullptr)
    {
        AMM::OperationalDescription od;
        od.name("disconnect");
        od.module_id(m_uuid);
        mgr->WriteOperationalDescription(od);
        mgr->Shutdown();
        delete mgr;
        mgr = nullptr;
    }

    LOG_INFO << "Shutdown complete";

    return 0;
}
