#include "contests.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
int main(int argc,char** argv) {
    try {
        bool dry=false;
        if (argc==2 && std::string(argv[1])=="--dry-run") dry=true;
        else if (argc!=1) throw std::runtime_error("Uso: contest-announcer [--dry-run]");
        if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK) throw std::runtime_error("Falha de inicialização HTTP");
        struct Cleanup { ~Cleanup(){curl_global_cleanup();} } cleanup;
        auto state=bot::load("data/state.json");
        const char* webhook=std::getenv("DISCORD_WEBHOOK_URL");
        if(!dry && (!webhook || !*webhook)) throw std::runtime_error("Cadastre o secret DISCORD_WEBHOOK_URL");
        std::vector<bot::Contest> contests; bool failed=false;
        for (const auto& platform : {"Codeforces","AtCoder"}) {
            try {
                auto list=std::string(platform)=="Codeforces" ? bot::codeforces(bot::get("https://codeforces.com/api/contest.list?gym=false")) : bot::atcoder(bot::get("https://atcoder.jp/contests/?lang=en"));
                std::cout << platform << ": " << list.size() << " contests carregados\n";
                contests.insert(contests.end(),list.begin(),list.end());
            } catch(const std::exception& e) { std::cerr << platform << ": " << e.what() << '\n'; failed=true; }
        }
        std::sort(contests.begin(),contests.end(),[](const auto& a,const auto& b){return a.start<b.start;});
        auto now=[] {return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();};
        bot::process(contests,state,now,[&](const bot::json& payload){
            if(dry) std::cout << payload.dump(2) << '\n'; else bot::post(webhook,payload);
        },[&]{if(!dry) bot::save("data/state.json",state);});
        return failed?1:0;
    } catch(const std::exception& e) {std::cerr << "Erro: " << e.what() << '\n'; return 1;}
}
