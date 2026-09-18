#include "contests.hpp"
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <regex>
#include <sstream>
#include <thread>
namespace bot {
std::string classify(const std::string& p, const std::string& name, const std::string& id) {
    if (p == "AtCoder") {
        for (auto prefix : {"abc", "arc", "agc", "ahc", "awc"}) {
            if (id.starts_with(prefix) && id.size() > 3 && std::isdigit(static_cast<unsigned char>(id[3]))) {
                std::string type(prefix);
                std::transform(type.begin(), type.end(), type.begin(), ::toupper);
                return type;
            }
        }
        return "Outro";
    }
    std::regex div(R"(Div\.?\s*([1-4]))", std::regex::icase);
    std::vector<std::string> divisions;
    for (std::sregex_iterator i(name.begin(), name.end(), div), end; i != end; ++i)
        if (std::find(divisions.begin(), divisions.end(), (*i)[1].str()) == divisions.end()) divisions.push_back((*i)[1]);
    std::sort(divisions.begin(), divisions.end());
    std::string result = name.find("Educational") != std::string::npos ? "Educational" : "";
    for (const auto& d : divisions) result += (result.empty() ? "" : " + ") + std::string("Div. ") + d;
    if (!result.empty()) return result;
    if (name.find("Global Round") != std::string::npos) return "Global Round";
    return "Outro";
}
std::vector<Contest> codeforces(const std::string& body) {
    auto j = json::parse(body);
    if (j.at("status") != "OK" || !j.at("result").is_array()) throw std::runtime_error("Resposta Codeforces inválida");
    std::vector<Contest> out;
    for (const auto& c : j.at("result")) {
        if (c.at("phase") != "BEFORE") continue;
        auto id = std::to_string(c.at("id").get<int64_t>());
        auto name = c.at("name").get<std::string>();
        out.push_back({"cf:"+id,"Codeforces",name,"https://codeforces.com/contest/"+id,
                       classify("Codeforces",name,id),c.at("startTimeSeconds"),c.at("durationSeconds")});
    }
    return out;
}
using Doc = std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)>;
using Context = std::unique_ptr<xmlXPathContext, decltype(&xmlXPathFreeContext)>;
using XPath = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>;
XPath query(xmlXPathContext* ctx, const char* expression) {
    XPath result(xmlXPathEvalExpression(BAD_CAST expression, ctx), xmlXPathFreeObject);
    if (!result) throw std::runtime_error("XPath inválido");
    return result;
}
std::string content(xmlNode* node) {
    auto raw = xmlNodeGetContent(node);
    if (!raw) throw std::runtime_error("Conteúdo HTML ausente");
    std::string s(reinterpret_cast<char*>(raw)); xmlFree(raw); return s;
}
std::string one(xmlXPathContext* ctx, const char* expression) {
    auto nodes = query(ctx,expression);
    if (!nodes->nodesetval || nodes->nodesetval->nodeNr != 1) throw std::runtime_error("Estrutura AtCoder alterada");
    return content(nodes->nodesetval->nodeTab[0]);
}
int64_t timestamp(const std::string& s) {
    std::smatch m;
    if (!std::regex_match(s,m,std::regex(R"((\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})([+-])(\d{2})(\d{2}))")))
        throw std::runtime_error("Data AtCoder inválida");
    using namespace std::chrono;
    year_month_day date{year{std::stoi(m[1])},month{static_cast<unsigned>(std::stoi(m[2]))},day{static_cast<unsigned>(std::stoi(m[3]))}};
    int h=std::stoi(m[4]), min=std::stoi(m[5]), sec=std::stoi(m[6]), zh=std::stoi(m[8]), zm=std::stoi(m[9]);
    if (!date.ok() || h>23 || min>59 || sec>59 || zh>23 || zm>59) throw std::runtime_error("Data fora do intervalo");
    auto utc = sys_days{date}+hours{h}+minutes{min}+seconds{sec} - minutes{(zh*60+zm)*(m[7]=="+"?1:-1)};
    return duration_cast<seconds>(utc.time_since_epoch()).count();
}
std::vector<Contest> atcoder(const std::string& body) {
    Doc doc(htmlReadMemory(body.data(),static_cast<int>(body.size()),nullptr,"UTF-8",HTML_PARSE_NONET|HTML_PARSE_NOERROR|HTML_PARSE_NOWARNING),xmlFreeDoc);
    if (!doc) throw std::runtime_error("HTML AtCoder inválido");
    Context ctx(xmlXPathNewContext(doc.get()),xmlXPathFreeContext);
    auto section=query(ctx.get(),"//*[@id='contest-table-upcoming']");
    if (!section->nodesetval || section->nodesetval->nodeNr!=1) throw std::runtime_error("Tabela AtCoder ausente");
    auto rows=query(ctx.get(),"//*[@id='contest-table-upcoming' or @id='contest-table-daily']//tbody/tr");
    std::vector<Contest> out;
    if (!rows->nodesetval) return out;
    for (int i=0;i<rows->nodesetval->nodeNr;++i) {
        ctx->node=rows->nodesetval->nodeTab[i];
        auto href=one(ctx.get(),"./td[2]/a[starts-with(@href,'/contests/')]/@href");
        auto name=one(ctx.get(),"./td[2]/a[starts-with(@href,'/contests/')]");
        auto id=href.substr(10);
        if (!std::regex_match(id,std::regex("[A-Za-z0-9_-]+"))) throw std::runtime_error("ID AtCoder inválido");
        auto time=timestamp(one(ctx.get(),"./td[1]//time"));
        auto duration=one(ctx.get(),"./td[3]"); std::smatch m;
        if (!std::regex_match(duration,m,std::regex(R"(\s*(\d+):(\d{2})\s*)")) || std::stoi(m[2])>59) throw std::runtime_error("Duração inválida");
        out.push_back({"ac:"+id,"AtCoder",name,"https://atcoder.jp"+href,classify("AtCoder",name,id),time,std::stoll(m[1])*3600+std::stoi(m[2])*60});
    }
    return out;
}
json load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Estado ausente ou ilegível; restaure data/state.json");
    json state; in >> state;
    if (state.at("version")!=1 || !state.at("sent").is_object()) throw std::runtime_error("Estado inválido");
    for (const auto& entry : state.at("sent")) {
        if (!entry.is_object()) throw std::runtime_error("Registro de estado inválido");
        for (const auto& value : entry) if (!value.is_number_integer()) throw std::runtime_error("Timestamp de estado inválido");
    }
    return state;
}
void save(const std::filesystem::path& path,const json& state) {
    auto temp=path; temp += ".tmp";
    { std::ofstream out(temp); out.exceptions(std::ios::failbit|std::ios::badbit); out << state.dump(2) << '\n'; out.close(); }
    std::filesystem::rename(temp,path);
}
std::string key(const Contest& c) { return c.id+":"+std::to_string(c.start); }
std::vector<std::string> due(const Contest& c,const json& state,int64_t now) {
    auto left=c.start-now;
    if (left<=0) return {};
    auto sent=state.at("sent").value(key(c),json::object());
    std::vector<std::string> result;
    if (!sent.contains("new")) result.push_back("new");
    // Look ahead one polling interval so the final reminder is not skipped.
    if (left<=4*3600 && !sent.contains("1h")) result.push_back("1h");
    else if (left>4*3600 && left<=27*3600 && !sent.contains("24h")) result.push_back("24h");
    return result;
}
json payload(const Contest& c,const std::string& kind) {
    auto stamp=std::to_string(c.start);
    std::string title=kind=="new"?"📢 Novo contest":kind=="24h"?"⏰ Lembrete de 24h (janela de consulta)":"⏰ Lembrete de 1h (janela de consulta)";
    return {{"allowed_mentions",{{"parse",json::array()}}},{"embeds",json::array({{
        {"title",title},{"description",c.name},{"url",c.url},{"color",c.platform=="Codeforces"?3447003:3066993},
        {"fields",json::array({{{"name","Plataforma"},{"value",c.platform},{"inline",true}},
          {{"name","Tipo / divisão"},{"value",c.type},{"inline",true}},
          {{"name","Início"},{"value","<t:"+stamp+":F>\n<t:"+stamp+":R>"}},
          {{"name","Duração"},{"value",std::to_string(c.duration/3600)+"h "+std::to_string(c.duration%3600/60)+"min"}}})},
        {"footer",{{"text","SpeedyCoders • consulta a cada 3h; lembretes podem ser antecipados"}}}
    }})}};
}
void process(const std::vector<Contest>& contests,json& state,const std::function<int64_t()>& now,
             const std::function<void(const json&)>& send,const std::function<void()>& checkpoint) {
    for (const auto& c : contests) for (const auto& kind : due(c,state,now())) {
        if (c.start<=now()) break;
        send(payload(c,kind));
        state["sent"][key(c)][kind]=now();
        checkpoint();
    }
}
size_t receive(char* data,size_t size,size_t count,void* ptr) {
    auto& body=*static_cast<std::string*>(ptr); auto n=size*count;
    if (body.size()+n>32*1024*1024) return 0;
    try { body.append(data,n); } catch (...) { return 0; }
    return n;
}
struct Response { long status; std::string body; };
Response request(const std::string& url,const std::string* payload) {
    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl(curl_easy_init(),curl_easy_cleanup);
    if (!curl) throw std::runtime_error("Falha ao iniciar HTTP");
    std::string body;
    curl_easy_setopt(curl.get(),CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl.get(),CURLOPT_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl.get(),CURLOPT_CONNECTTIMEOUT,15L);
    curl_easy_setopt(curl.get(),CURLOPT_TIMEOUT,45L);
    curl_easy_setopt(curl.get(),CURLOPT_USERAGENT,"SpeedyCoders-Contest-Announcer/1.0");
    curl_easy_setopt(curl.get(),CURLOPT_WRITEFUNCTION,receive);
    curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&body);
    std::unique_ptr<curl_slist,decltype(&curl_slist_free_all)> headers(nullptr,curl_slist_free_all);
    if (payload) {
        headers.reset(curl_slist_append(nullptr,"Content-Type: application/json"));
        curl_easy_setopt(curl.get(),CURLOPT_HTTPHEADER,headers.get());
        curl_easy_setopt(curl.get(),CURLOPT_POSTFIELDS,payload->c_str());
        curl_easy_setopt(curl.get(),CURLOPT_POSTFIELDSIZE,static_cast<long>(payload->size()));
    }
    if (curl_easy_perform(curl.get())!=CURLE_OK) throw std::runtime_error("Falha de transporte HTTP (URL omitida)");
    long status=0; curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&status);
    return {status,body};
}
std::string get(const std::string& url) {
    for (int i=0;i<3;++i) {
        try { auto r=request(url,nullptr); if(r.status==200) return r.body; } catch (const std::exception&) { if(i==2) throw; }
        if (i<2) std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    throw std::runtime_error("Consulta HTTP falhou após 3 tentativas");
}
void post(const std::string& webhook,const json& body) {
    if (!std::regex_match(webhook,std::regex(R"(https://(canary\.|ptb\.)?discord\.com/api/webhooks/[0-9]+/[A-Za-z0-9._-]+)")))
        throw std::runtime_error("DISCORD_WEBHOOK_URL inválida; use a URL padrão sem /github ou parâmetros");
    auto encoded=body.dump();
    for (int i=0;i<4;++i) {
        auto r=request(webhook+"?wait=true",&encoded);
        if (r.status>=200 && r.status<300) { std::this_thread::sleep_for(std::chrono::milliseconds(500)); return; }
        if (r.status!=429 || i==3) throw std::runtime_error("Discord HTTP "+std::to_string(r.status));
        double delay=json::parse(r.body).at("retry_after").get<double>();
        if (!(delay>=0 && delay<=60)) throw std::runtime_error("Discord solicitou espera longa; tente na próxima execução");
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay*1000)+250));
    }
}
}
