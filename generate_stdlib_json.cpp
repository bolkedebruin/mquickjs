/*
 * Generate stdlib.json from MQuickJS C structures
 *
 * This program:
 * 1. Includes actual JSPropDef structures from stdlib (deterministic)
 * 2. Uses regex to parse JSDoc comments for documentation (flexible)
 * 3. Uses nlohmann/json for robust JSON parsing and generation
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <algorithm>
#include <cctype>

#include "json.hpp"

// Include mquickjs build infrastructure with C linkage
extern "C" {
#include "mquickjs_build.h"
}

using namespace std;
using json = nlohmann::json;

// Stub out function symbols that are only used as function pointers in JSPropDef
// We only need the structures, not the implementations
#define STUB_FUNC(name) extern "C" void* name = nullptr


// ============================================================================
// Documentation Parser (uses regex for JSDoc comments)
// ============================================================================

struct ParamDoc {
    string name;
    string type;
    string description;
};

struct FuncDoc {
    string description;
    vector<ParamDoc> params;
    string returnType;
};

class JSDocParser {
private:
    regex jsdoc_block_regex{R"(/\*\*([\s\S]*?)\*/)"};
    regex jsapi_regex{R"(@jsapi\s+(\S+))"};
    regex description_regex{R"(@description\s+([\s\S]+?)(?=@|\*/|$))"};
    regex param_regex{R"(@param\s+\{([^}]+)\}\s+(\w+)\s*-?\s*(.*))"};
    regex returns_regex{R"(@returns\s+\{([^}]+)\})"};

    string trim(const string& str) {
        auto start = find_if_not(str.begin(), str.end(), [](unsigned char ch) {
            return isspace(ch);
        });
        auto end = find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
            return isspace(ch);
        }).base();
        return (start < end) ? string(start, end) : "";
    }

    string cleanWhitespace(const string& str) {
        string result;
        bool prevSpace = false;
        for (char ch : str) {
            if (isspace(ch) || ch == '*') {  // Also remove asterisks from JSDoc
                if (!prevSpace && !result.empty() && ch != '*') {
                    result += ' ';
                }
                prevSpace = true;
            } else {
                result += ch;
                prevSpace = false;
            }
        }
        return result;
    }

public:
    map<string, FuncDoc> parseFile(const string& filename) {
        map<string, FuncDoc> docs;

        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Warning: Could not open " << filename << " for documentation" << endl;
            return docs;
        }

        stringstream buffer;
        buffer << file.rdbuf();
        string content = buffer.str();

        auto blocks_begin = sregex_iterator(content.begin(), content.end(), jsdoc_block_regex);
        auto blocks_end = sregex_iterator();

        for (auto it = blocks_begin; it != blocks_end; ++it) {
            string block = it->str(1);
            smatch match;

            // Extract @jsapi to get the method name
            string methodName;
            if (regex_search(block, match, jsapi_regex)) {
                string fullName = match[1];
                // Extract method name after the dot (e.g., "led.count" -> "count")
                size_t dotPos = fullName.find('.');
                if (dotPos != string::npos) {
                    methodName = fullName.substr(dotPos + 1);
                }
            }

            if (methodName.empty()) continue;

            FuncDoc doc;

            // Extract @description
            if (regex_search(block, match, description_regex)) {
                doc.description = trim(cleanWhitespace(match[1]));
            }

            // Extract @param tags
            auto params_begin = sregex_iterator(block.begin(), block.end(), param_regex);
            auto params_end = sregex_iterator();
            for (auto pit = params_begin; pit != params_end; ++pit) {
                ParamDoc param;
                param.type = trim(pit->str(1));
                param.name = trim(pit->str(2));
                param.description = trim(pit->str(3));
                doc.params.push_back(param);
            }

            // Extract @returns
            if (regex_search(block, match, returns_regex)) {
                doc.returnType = trim(match[1]);
            }

            docs[methodName] = doc;
        }

        return docs;
    }
};

// ============================================================================
// Include stdlib definitions from actual source files
// ============================================================================

// Stub out all functions referenced in JSPropDef arrays
STUB_FUNC(js_freebutton_led_count);
STUB_FUNC(js_freebutton_led_on);
STUB_FUNC(js_freebutton_led_off);
STUB_FUNC(js_freebutton_led_setColor);
STUB_FUNC(js_freebutton_button_count);
STUB_FUNC(js_freebutton_button_setLabel);
STUB_FUNC(js_freebutton_button_setTopLabel);
STUB_FUNC(js_freebutton_button_onClick);
STUB_FUNC(js_freebutton_button_onLongPress);
STUB_FUNC(js_freebutton_button_onRelease);
STUB_FUNC(js_freebutton_sensor_count);
STUB_FUNC(js_freebutton_sensor_getValue);
STUB_FUNC(js_freebutton_sensor_getType);
STUB_FUNC(js_freebutton_sensor_getInfo);
STUB_FUNC(js_freebutton_sensor_getAll);
STUB_FUNC(js_freebutton_sensor_onChange);
STUB_FUNC(js_freebutton_mqtt_getBrokerCount);
STUB_FUNC(js_freebutton_mqtt_getBrokerName);
STUB_FUNC(js_freebutton_mqtt_isConnected);
STUB_FUNC(js_freebutton_mqtt_publish);
STUB_FUNC(js_freebutton_mqtt_subscribe);
STUB_FUNC(js_freebutton_mqtt_unsubscribe);
STUB_FUNC(js_freebutton_mqtt_onConnect);
STUB_FUNC(js_freebutton_mqtt_onDisconnect);

// Stub global functions
STUB_FUNC(js_print);
STUB_FUNC(js_number_parseInt);
STUB_FUNC(js_number_parseFloat);
STUB_FUNC(js_global_eval);
STUB_FUNC(js_global_isNaN);
STUB_FUNC(js_global_isFinite);
STUB_FUNC(js_setTimeout);
STUB_FUNC(js_clearTimeout);
STUB_FUNC(js_gc);
STUB_FUNC(js_load);
STUB_FUNC(js_loadMapped);
STUB_FUNC(js_performance_now);

// Skip the main() function when including stdlib files
#define main __unused_main

// Include the actual structure definitions from freebutton_stdlib.c
#include "freebutton_stdlib.c"

#undef main

// ============================================================================
// Structure Walker
// ============================================================================

class StructureWalker {
private:
    JSDocParser docParser;
    map<string, map<string, FuncDoc>> apiDocs; // apiName -> methodName -> FuncDoc

    // Docs loaded from mqjs_stdlib_docs.json
    json classDocsJson;
    json globalDocsJson;

    void loadMqjsDocsJSON(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Warning: Could not open " << filename << endl;
            return;
        }

        try {
            json docs = json::parse(file);

            // Store class docs
            for (auto& [className, classDef] : docs.items()) {
                if (className != "globals") {
                    classDocsJson[className] = classDef;
                }
            }

            // Store global docs
            if (docs.contains("globals")) {
                globalDocsJson = docs["globals"];
            }

            cerr << "Loaded " << classDocsJson.size() << " classes from " << filename << endl;
            for (auto& [className, classDef] : classDocsJson.items()) {
                int methodCount = 0;
                if (classDef.contains("static")) methodCount += classDef["static"].size();
                if (classDef.contains("prototype")) methodCount += classDef["prototype"].size();
                cerr << "  " << className << ": " << methodCount << " methods" << endl;
            }

            if (!globalDocsJson.empty()) {
                cerr << "Loaded " << globalDocsJson.size() << " global functions" << endl;
            }

        } catch (const json::exception& e) {
            cerr << "Error parsing " << filename << ": " << e.what() << endl;
        }
    }

public:
    void parseDocumentation(const string& apiName, const string& filename) {
        apiDocs[apiName] = docParser.parseFile(filename);
    }

    void loadClassDocs(const string& filename) {
        loadMqjsDocsJSON(filename);
    }

    // Convert class-style docs to API docs (for console, performance)
    void convertClassToAPIDocs(const string& className, const string& apiName) {
        if (classDocsJson.contains(className) && classDocsJson[className].contains("static")) {
            apiDocs[apiName] = {};
            for (auto& [methodName, methodDoc] : classDocsJson[className]["static"].items()) {
                FuncDoc doc;
                if (methodDoc.contains("description")) {
                    doc.description = methodDoc["description"].get<string>();
                }
                if (methodDoc.contains("returns")) {
                    doc.returnType = methodDoc["returns"].get<string>();
                }
                if (methodDoc.contains("params")) {
                    for (auto& param : methodDoc["params"]) {
                        ParamDoc p;
                        p.name = param["name"].get<string>();
                        p.type = param["type"].get<string>();
                        if (param.contains("description")) {
                            p.description = param["description"].get<string>();
                        }
                        doc.params.push_back(p);
                    }
                }
                apiDocs[apiName][methodName] = doc;
            }
        }
    }

    json emitMethods(const JSPropDef* props, const string& className, const string& section) {
        json methods = json::array();

        if (!props) return methods;

        for (const JSPropDef* prop = props; prop->def_type != JS_DEF_END; prop++) {
            if (prop->def_type == JS_DEF_CFUNC) {
                string name = prop->name ? prop->name : "unknown";

                json method = {
                    {"name", name},
                    {"description", ""},
                    {"params", json::array()},
                    {"returns", "any"}
                };

                // Look up documentation
                if (classDocsJson.contains(className) &&
                    classDocsJson[className].contains(section) &&
                    classDocsJson[className][section].contains(name)) {

                    auto& doc = classDocsJson[className][section][name];

                    if (doc.contains("description")) {
                        method["description"] = doc["description"];
                    }
                    if (doc.contains("returns")) {
                        method["returns"] = doc["returns"];
                    }
                    if (doc.contains("params")) {
                        method["params"] = doc["params"];
                    }
                } else {
                    cerr << "Warning: No docs for " << className << "." << section << "." << name << endl;
                }

                methods.push_back(method);
            }
        }

        return methods;
    }

    void validateDocs(const string& className, const string& section, const JSPropDef* props) {
        // Check if docs exist but method doesn't
        if (classDocsJson.contains(className) && classDocsJson[className].contains(section)) {
            for (auto& [methodName, doc] : classDocsJson[className][section].items()) {
                bool found = false;
                if (props) {
                    for (const JSPropDef* prop = props; prop->def_type != JS_DEF_END; prop++) {
                        if (prop->def_type == JS_DEF_CFUNC && prop->name && methodName == prop->name) {
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    cerr << "Warning: Docs exist but method not found: " << className << "." << section << "." << methodName << endl;
                }
            }
        }
    }

    json emitClass(const string& className, const JSPropDef* staticProps, const JSPropDef* protoProps) {
        json classObj = json::object();

        // Validate documentation
        validateDocs(className, "static", staticProps);
        validateDocs(className, "prototype", protoProps);

        // Static methods
        if (staticProps && staticProps->def_type != JS_DEF_END) {
            classObj["static"] = emitMethods(staticProps, className, "static");
        }

        // Prototype methods
        if (protoProps && protoProps->def_type != JS_DEF_END) {
            classObj["prototype"] = emitMethods(protoProps, className, "prototype");
        }

        return classObj;
    }

    json emitAPI(const string& apiName, const JSPropDef* props) {
        json api = {
            {"type", "object"},
            {"methods", json::array()}
        };

        for (const JSPropDef* prop = props; prop->def_type != JS_DEF_END; prop++) {
            if (prop->def_type == JS_DEF_CFUNC) {
                json method = {
                    {"name", prop->name},
                    {"description", ""},
                    {"params", json::array()},
                    {"returns", "void"}
                };

                // Look up documentation for this method
                if (apiDocs.count(apiName) && apiDocs[apiName].count(prop->name)) {
                    const FuncDoc& doc = apiDocs[apiName][prop->name];
                    method["description"] = doc.description;
                    method["returns"] = doc.returnType.empty() ? "void" : doc.returnType;

                    for (const auto& param : doc.params) {
                        json paramObj = {
                            {"name", param.name},
                            {"type", param.type}
                        };
                        if (!param.description.empty()) {
                            paramObj["description"] = param.description;
                        }
                        method["params"].push_back(paramObj);
                    }
                }

                api["methods"].push_back(method);
            }
        }

        return api;
    }

    json emitGlobals(const JSPropDef* props) {
        json globals = json::array();

        if (!props) return globals;

        for (const JSPropDef* prop = props; prop->def_type != JS_DEF_END; prop++) {
            if (prop->def_type == JS_DEF_CFUNC) {
                string name = prop->name ? prop->name : "unknown";

                // Skip these - they're not global functions but classes/objects
                if (name == "led" || name == "button" || name == "sensor" ||
                    name == "mqtt" || name == "console" || name == "performance") {
                    continue;
                }

                json global = {
                    {"name", name},
                    {"description", ""},
                    {"params", json::array()},
                    {"returns", "any"}
                };

                // Look up documentation
                if (globalDocsJson.contains(name)) {
                    auto& doc = globalDocsJson[name];

                    if (doc.contains("description")) {
                        global["description"] = doc["description"];
                    }
                    if (doc.contains("returns")) {
                        global["returns"] = doc["returns"];
                    }
                    if (doc.contains("params")) {
                        global["params"] = doc["params"];
                    }
                }

                globals.push_back(global);
            }
        }

        return globals;
    }

    void generateJSON(ostream& out) {
        json output = json::object();

        // APIs section
        json apis = json::object();

        // FreeButton APIs
        extern const JSPropDef js_freebutton_button[];
        extern const JSPropDef js_freebutton_sensor[];
        extern const JSPropDef js_freebutton_mqtt[];

        apis["led"] = emitAPI("led", js_freebutton_led);
        apis["button"] = emitAPI("button", js_freebutton_button);
        apis["sensor"] = emitAPI("sensor", js_freebutton_sensor);
        apis["mqtt"] = emitAPI("mqtt", js_freebutton_mqtt);

        // MQuickJS stdlib APIs
        extern const JSPropDef js_console[];
        extern const JSPropDef js_performance[];

        apis["console"] = emitAPI("console", js_console);
        apis["performance"] = emitAPI("performance", js_performance);

        output["apis"] = apis;

        // Classes section - emit standard library classes
        json classes = json::object();

        extern const JSPropDef js_object[];
        extern const JSPropDef js_object_proto[];
        classes["Object"] = emitClass("Object", js_object, js_object_proto);

        extern const JSPropDef js_array[];
        extern const JSPropDef js_array_proto[];
        classes["Array"] = emitClass("Array", js_array, js_array_proto);

        extern const JSPropDef js_string[];
        extern const JSPropDef js_string_proto[];
        classes["String"] = emitClass("String", js_string, js_string_proto);

        extern const JSPropDef js_number[];
        extern const JSPropDef js_number_proto[];
        classes["Number"] = emitClass("Number", js_number, js_number_proto);

        extern const JSPropDef js_math[];
        classes["Math"] = emitClass("Math", js_math, nullptr);

        extern const JSPropDef js_json[];
        classes["JSON"] = emitClass("JSON", js_json, nullptr);

        // Boolean class (constructor only, no static/proto methods we can extract)
        classes["Boolean"] = emitClass("Boolean", nullptr, nullptr);

        // Date class (has static methods)
        extern const JSPropDef js_date[];
        classes["Date"] = emitClass("Date", js_date, nullptr);

        // Function class (has prototype methods)
        extern const JSPropDef js_function_proto[];
        classes["Function"] = emitClass("Function", nullptr, js_function_proto);

        // RegExp class (has prototype methods)
        extern const JSPropDef js_regexp_proto[];
        classes["RegExp"] = emitClass("RegExp", nullptr, js_regexp_proto);

        // Error class (has prototype methods)
        extern const JSPropDef js_error_proto[];
        classes["Error"] = emitClass("Error", nullptr, js_error_proto);

        output["classes"] = classes;

        // Globals section
        extern const JSPropDef js_global_object[];
        output["globals"] = emitGlobals(js_global_object);

        // Constants section
        output["constants"] = json::array({
            {
                {"name", "undefined"},
                {"type", "undefined"},
                {"description", "The undefined value"}
            },
            {
                {"name", "Infinity"},
                {"type", "number"},
                {"description", "Positive infinity value"}
            },
            {
                {"name", "NaN"},
                {"type", "number"},
                {"description", "Not-a-Number value"}
            },
            {
                {"name", "globalThis"},
                {"type", "object"},
                {"description", "The global object"}
            }
        });

        // Output with pretty printing (2-space indent)
        out << output.dump(2) << endl;
    }
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    StructureWalker walker;

    // Load class documentation
    walker.loadClassDocs("mqjs_stdlib_docs.json");

    // Convert console and performance from class docs to API docs
    walker.convertClassToAPIDocs("console", "console");
    walker.convertClassToAPIDocs("performance", "performance");

    // Parse documentation from source files (using regex)
    walker.parseDocumentation("led", "freebutton_led.c");
    walker.parseDocumentation("button", "freebutton_button.c");
    walker.parseDocumentation("sensor", "freebutton_sensor.c");
    walker.parseDocumentation("mqtt", "freebutton_mqtt.c");

    // Generate JSON output (walking structures directly)
    walker.generateJSON(cout);

    return 0;
}
