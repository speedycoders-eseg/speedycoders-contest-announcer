#include "contests.hpp"
#include <fstream>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace bot;
void check(bool ok) { if(!ok) throw std::runtime_error("Teste falhou"); }
template<class F> void throws(F fn) {bool caught=false;try{fn();}catch(const std::exception&){caught=true;}check(caught);}
int main() {
    check(normalize_webhook("https://discordapp.com/api/webhooks/123/test-token") == "https://discord.com/api/webhooks/123/test-token");
    check(normalize_webhook("https://discord.com/api/webhooks/123/test-token") == "https://discord.com/api/webhooks/123/test-token");
    check(normalize_webhook("https://canary.discordapp.com/api/webhooks/123/test-token") == "https://canary.discord.com/api/webhooks/123/test-token");
    throws([]{normalize_webhook("https://discordapp.com.evil.com/api/webhooks/123/test-token");});
    throws([]{normalize_webhook("https://discordapp.com/api/webhooks/123/test-token/github");});
    throws([]{normalize_webhook("https://discordapp.com/api/webhooks");});
    check(classify("Codeforces","Round (Div. 1 + Div. 2)","")=="Div. 1 + Div. 2");
    check(classify("Codeforces","Educational Round (Rated for Div. 2)","")=="Educational + Div. 2");
    check(classify("Codeforces","Round Div.4","")=="Div. 4");
    check(classify("Codeforces","Unknown","")=="Outro");
    check(classify("AtCoder","Sponsor ABC","abc476")=="ABC");
    check(classify("AtCoder","Regular++","arc230")=="ARC");
    check(classify("AtCoder","Unknown","sponsor")=="Outro");
    auto cf=codeforces(R"JSON({"status":"OK","result":[{"id":10,"name":"Round (Div. 3)","phase":"BEFORE","startTimeSeconds":200000,"durationSeconds":7200},{"phase":"FINISHED"}]})JSON");
    check(cf.size()==1 && cf[0].type=="Div. 3");
    throws([]{codeforces(R"({"status":"FAILED"})");});
    auto ac=atcoder(R"(<html><div id="contest-table-upcoming"><table><tbody><tr><td><time>2026-09-19 21:00:00+0900</time></td><td><a href="/contests/abc476">Sponsor &amp; ABC</a></td><td>240:00</td><td>1999</td></tr></tbody></table></div><div id="contest-table-recent"><table><tbody><tr><td>ignored</td></tr></tbody></table></div></html>)");
    check(ac.size()==1 && ac[0].name=="Sponsor & ABC" && ac[0].duration==864000);
    using namespace std::chrono;
    check(ac[0].start==duration_cast<seconds>((sys_days{year{2026}/9/19}+hours{12}).time_since_epoch()).count());
    throws([]{atcoder("<html>challenge page</html>");});
    check(atcoder("<div id='contest-table-upcoming'><table><tbody></tbody></table></div>").empty());
    json state={{"version",1},{"sent",json::object()}};
    Contest c={"cf:1","Codeforces","Test","https://codeforces.com/contest/1","Div. 2",200000,7200};
    check(due(c,state,c.start).empty());
    check(due(c,state,c.start-27*3600-1)==std::vector<std::string>{"new"});
    check(due(c,state,c.start-27*3600)==(std::vector<std::string>{"new","24h"}));
    check(due(c,state,c.start-4*3600-1)==(std::vector<std::string>{"new","24h"}));
    check(due(c,state,c.start-4*3600)==(std::vector<std::string>{"new","1h"}));
    check(due(c,state,c.start-1)==(std::vector<std::string>{"new","1h"}));
    int sends=0, saves=0; auto now=[&]{return c.start-3600;};
    throws([&]{process({c},state,now,[&](auto){if(++sends==2)throw std::runtime_error("Discord failed");},[&]{++saves;});});
    check(saves==1 && due(c,state,now())==std::vector<std::string>{"1h"});
    process({c},state,now,[&](auto){++sends;},[&]{++saves;});
    check(sends==3 && saves==2 && due(c,state,now()).empty());
    process({c,c},state,now,[&](auto){++sends;},[&]{++saves;});
    check(sends==3);
    c.start+=7200; check(due(c,state,now()).size()==2); // rescheduled contest
    auto path=std::filesystem::temp_directory_path()/"speedycoders-test-state.json";
    save(path,state); check(load(path)==state);
    {std::ofstream out(path);out << "{broken";} throws([&]{load(path);});
    std::filesystem::remove(path); throws([&]{load(path);});
    check(payload(c,"new")["allowed_mentions"]["parse"].empty());
    std::cout << "Todos os testes passaram\n";
}
