#include "common/repository.hpp"
#include "common/connection.hpp"
#include "common/protocol.hpp"
#include <iostream>
#include <cstdio>
#include <thread>
#include <atomic>
#include <unordered_map>

using namespace std;

struct Server{
    bool IsRunning = true;
    std::vector<Repository> Repositories;
    TcpListener ConnectionListener;

    std::unordered_map<IpAddress, Connection> Connections;

    Server(){
        ConnectionListener.listen(s_DefaultServerPort);
        ConnectionListener.setBlocking(false);
    }

    bool CreateRepository(fs::path path, string name){
        if(!fs::exists(path))
            fs::create_directories(path);
        
        //if(!fs::is_empty(path)){assert(false); return false;}
 
        Repositories.emplace_back(move(name), move(path));

        return true; 
    }

    bool OpenRepository(fs::path path, string name){
        if(!fs::exists(path))return false;

        Repositories.emplace_back(move(name), move(path));
        Repositories.back().UpdateState();

        return true;
    }

    void CheckPendingConnections(){
        Connection connection;
        if(ConnectionListener.accept(connection) == Socket::Done){

            std::cout << "[Connected]: " << connection.getRemoteAddress() << ":" << connection.getRemotePort() << std::endl;

            auto remote = connection.getRemoteAddress();

            Connections.emplace(connection.getRemoteAddress(), std::move(connection));
        }
    }

    void CheckPendingRequests(){
        for(auto &c: Connections){
            
            sf::Packet packet;
            if(c.second.receive(packet) == Socket::Done)
                std::cout << "Connection: "  << c.second.getRemoteAddress() << ":" << c.second.getRemotePort() << " has sent " << packet.getDataSize() << " bytes\n";
        }
    }

    void PollRepositoriesState(){
        for(auto &repo: Repositories){
            auto ops = repo.UpdateState();
            if(ops.size())
                PushChanges(repo);
        }
    }

    void PushChanges(const Repository &repo){
        std::cout << "Pushing Changes\n";
        Packet packet;

        Header header;
        header.MagicWord = s_MagicWord;
        header.Type = MsgType::RepositoryStateNotify;

        packet << header;

        RepositoryStateNotify notify;
        notify.State = repo.LastState; //excessive copy

        packet << notify;

        for(auto &c: Connections){
            //we have to clone packet because for some reason SFML does not allow to reuse them
            Packet clone = packet;
            c.second.send(clone);
        }
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
    fs::path path = "/home/hephaestus/Dev/Cortex/RunDir";

    Server server;
    server.OpenRepository("/home/hephaestus/Dev/Cortex/RunDir/TestRepoStorage", "TestRepoName");

    server.Run();
}