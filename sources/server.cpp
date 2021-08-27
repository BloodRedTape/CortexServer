#include "common/repository.hpp"
#include "common/connection.hpp"
#include "common/protocol.hpp"
#include <iostream>
#include <cstdio>
#include <thread>

using namespace std;

struct Server{
    vector<Repository> Repositories;

    TcpListener Listener;
    vector<Connection> Connections;

    bool CreateRepository(fs::path path, string name){
        if(!fs::exists(path))
            fs::create_directories(path);
        
        if(!fs::is_empty(path)){assert(false); return false;}
            
        Repositories.emplace_back(move(name), move(path));

        return true; 
    }

    void Run(){
        Listener.listen(s_DefaultServerPort);
        Listener.setBlocking(false);

        for(;;){
            UpdateState();
            CheckPendingConnections();
        }
    }

    void CheckPendingConnections(){
        Connection connection;
        if(Listener.accept(connection) == Socket::Done){
            std::cout << "[Connected]: " << connection.getRemoteAddress() << ":" << connection.getRemotePort() << std::endl;

            Connections.push_back(move(connection));
        }
    }

    void UpdateState(){
        for(auto &repo: Repositories){
            auto ops = repo.UpdateState();
            if(ops.size())
                PushChanges(ops);
        }
    }

    void PushChanges(std::vector<RepositoryOperation> operations){
        std::cout << "StateChanges: " << std::endl;
        for(const auto &op: operations)
            std::cout << (int)op.Type << " : " << op.RelativeFilePath << std::endl;
    }

};

int main(){
    fs::path path = "/home/hephaestus/Dev/Cortex/RunDir";

    Server server;
    server.CreateRepository("/home/hephaestus/Dev/Cortex/RunDir/TestRepoStorage", "TestRepoName");

    for(;;){
        server.UpdateState();
        std::this_thread::sleep_for(5s);
    }
}