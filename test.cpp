#include <iostream>
#include "./Core/Timer.h"
#include "./Core/IdWorker.h"

using namespace snowflake;

int main(int argc, char **argv) {
    std::cout << "start generate id" << std::endl;

    auto &idWorker = Singleton<IdWorker>::instance();
    idWorker.setCamId(12);
    idWorker.setWorkerId(5);

    const size_t count = 20000000;

    Timer<> timer;
    for (size_t i = 0; i < count; i++)
    {
        
        std::cout<< "id[" << i << "]:" << idWorker.nextId()<<std::endl;
    }
    // 我的电脑生成 20000000 id 的耗时为 4.887s
    std::cout << "generate " << count << " id elapsed: " << timer.elapsed() << "ms" << std::endl;
    return 0;
}