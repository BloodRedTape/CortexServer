#include "common/repository.hpp"
#include "common/connection.hpp"
#include "common/protocol.hpp"
#include "fmt/printf.h"
#include <cstdio>
#include <thread>
#include <atomic>
#include <unordered_map>
#include "yaml-cpp/yaml.h"

using namespace std;

struct Server{
    bool IsRunning = true;
    RepositoriesRegistry Registry;
    TcpListener ConnectionListener;

    std::unordered_map<IpAddress, Connection> Connections;

    Server(){
        ConnectionListener.listen(s_DefaultServerPort);
        ConnectionListener.setBlocking(false);
    }

    bool Init(const char *init_filename = "init.yaml"){
        constexpr const char *Open = "Open";
        constexpr const char *Name = "Name";
        constexpr const char *Path = "Path";

        if(!fs::exists(init_filename)){
            fmt::print("File '{}' does not exist\n", init_filename);
            return false;
        }
        YAML::Node config = YAML::LoadFile(init_filename);

        if(config[Open]){
            for(const YAML::Node &repo: config[Open]){
                if(repo[Name] && repo[Path])
                    Registry.OpenRepository(repo[Path].as<std::string>(), repo[Name].as<std::string>());
                else
                    fmt::print("Open: Repo is ill-formated\n");
            }
        }


        return true;
    }

    void CheckPendingConnections(){
        Connection connection;
        if(ConnectionListener.accept(connection) == Socket::Done){
            
            fmt::print("[Connected]: {}:{}\n", connection.getRemoteAddress().toString(), connection.getRemotePort());

            auto remote = connection.getRemoteAddress();

            SendRepositoriesInfo(connection, Registry);

            Connections.emplace(connection.getRemoteAddress(), std::move(connection));
        }
    }

    static void SendRepositoriesInfo(Connection &connection, const RepositoriesRegistry &registry){
        Packet packet;

        Header header;
        header.MagicWord = s_MagicWord;
        header.Type = MsgType::RepositoriesInfo;

        packet << header;

        RepositoriesInfo info; // XXX excess copying
        info.RepositoryNames.reserve(registry.Repositories.size());
        for(const auto &repo: registry.Repositories)
            info.RepositoryNames.push_back(repo.first);
        
        packet << info;

        connection.send(packet);
    }

    void CheckPendingRequests(){
        for(auto &c: Connections){
            
            sf::Packet packet;
            if(c.second.receive(packet) == Socket::Done)
                fmt::print("Connection: {}:{} has sent {} bytes\n", c.second.getRemoteAddress().toString(), c.second.getRemotePort(), packet.getDataSize());
        }
    }

    void PollRepositoriesState(){
        for(auto &[name, repo]: Registry.Repositories){
            auto ops = repo.UpdateState();
            if(ops.size())
                PushChanges(name, repo);
        }
    }

    static void SendRepositoryState(Connection &connection, const std::string &name, const RepositoryState &state){
        Packet packet;

        Header header;
        header.MagicWord = s_MagicWord;
        header.Type = MsgType::RepositoryStateNotify;

        packet << header;

        RepositoryStateNotify notify;//XXX excessive copy// we can serialize Repository instead of this shit
        notify.RepositoryName = name;
        notify.RepositoryState = state; 

        packet << notify;

        connection.send(packet);
    }

    void PushChanges(const std::string &name, const Repository &repo){
        fmt::print("Pushing changes\n");

        for(auto &c: Connections)
            SendRepositoryState(c.second, name, repo.LastState);
    }

    void Run(){
        while(IsRunning){
            CheckPendingConnections();
            CheckPendingRequests();

            PollRepositoriesState();

            std::this_thread::sleep_for(1s);
        }
    }
};

int main(){
    Server server;
    if(server.Init())
        server.Run();
}