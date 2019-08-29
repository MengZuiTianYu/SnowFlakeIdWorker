#ifndef _JW_CORE_ID_WORKER_H_
#define _JW_CORE_ID_WORKER_H_

#include <mutex>
#include <atomic>
#include <chrono>
#include <exception>
#include <sstream>
#include "Noncopyable.h"
#include "Singleton.h"

// 如果不使用 mutex, 则开启下面这个定义, 但是我发现, 还是开启 mutex 功能, 速度比较快
// #define SNOWFLAKE_ID_WORKER_NO_LOCK

namespace snowflake {
    /**
        * 64bit id: 0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000 
        *           ||                                                     ||    　 | |         | |               | 
        *           |└-----------------------时间戳-------------- ----------┘└－机器-┘　└--CamID--┘ └----序列号-  ---┘ 
        *           |
        *         不用        
                   1                           37                               6          8             12
        * SnowFlake的优点: 整体上按照时间自增排序, 并且整个分布式系统内不会产生ID碰撞(由数据中心ID和机器ID作区分), 并且效率较高, 经测试, SnowFlake每秒能够产生26万ID左右.
        */
    class SnowflakeIdWorker : private Noncopyable {

        // 实现单例
        friend class Singleton<SnowflakeIdWorker>;

    public:
        typedef unsigned int UInt;
        typedef unsigned long long int UInt64;

#ifdef SNOWFLAKE_ID_WORKER_NO_LOCK
        typedef std::atomic<UInt> AtomicUInt;
        typedef std::atomic<UInt64> AtomicUInt64;
#else
        typedef UInt AtomicUInt;
        typedef UInt64 AtomicUInt64;
#endif

        int setWorkerId(UInt workerId) {
            if(workerId > maxWorkerId) {
                this->workerId = workerId % maxWorkerId;
                return -1;
            }
            else{
                this->workerId = workerId;
                return 0;
            }
        }

        int setCamId(UInt camId) {
            if(camId > maxCamId) {
                this->camId = camId & maxCamId;
                return -1;
            }else{
                this->camId = camId;
                return 0;
            }
        }

        UInt64 getId() {
            return nextId();
        }

        /**
            * 获得下一个ID (该方法是线程安全的)
            *
            * @return SnowflakeId
            */
        UInt64 nextId() {
#ifndef SNOWFLAKE_ID_WORKER_NO_LOCK
            std::unique_lock<std::mutex> lock{ mutex };
            AtomicUInt64 timestamp{ 0 };
#else
            static AtomicUInt64 timestamp{ 0 };
#endif
            timestamp = timeGen();

            // 如果当前时间小于上一次ID生成的时间戳，说明系统时钟回退过这个时候应当抛出异常
            if (timestamp < lastTimestamp) {
                std::ostringstream s;
                s << "clock moved backwards.  Refusing to generate id for " << lastTimestamp - timestamp << " milliseconds";
                throw std::exception(std::runtime_error(s.str()));
            }

            if (lastTimestamp == timestamp) {
                // 如果是同一时间生成的，则进行毫秒内序列
                sequence = (sequence + 1) & sequenceMask;
                if (0 == sequence) {
                    // 毫秒内序列溢出, 阻塞到下一个毫秒,获得新的时间戳
                    timestamp = tilNextMillis(lastTimestamp);
                }
            } else {
                sequence = 0;
            }

#ifndef SNOWFLAKE_ID_WORKER_NO_LOCK
            lastTimestamp = timestamp;
#else
            lastTimestamp = timestamp.load();
#endif

            // 移位并通过或运算拼到一起组成64位的ID
            return ((timestamp - twepoch) << timestampLeftShift)
            | (workerId << workerIdShift)
            | (camId << camIdShift)
            | sequence;
        }

    protected:
        SnowflakeIdWorker() : workerId(0), camId(0), sequence(0), lastTimestamp(0) { }

        /**
            * 返回以毫秒为单位的当前时间
            *
            * @return 当前时间(毫秒)
            */
        UInt64 timeGen() const {
            auto t = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now());
            return t.time_since_epoch().count();
        }

        /**
            * 阻塞到下一个毫秒，直到获得新的时间戳
            *
            * @param lastTimestamp 上次生成ID的时间截
            * @return 当前时间戳
            */
        UInt64 tilNextMillis(UInt64 lastTimestamp) const {
            UInt64 timestamp = timeGen();
            while (timestamp <= lastTimestamp) {
                timestamp = timeGen();
            }
            return timestamp;
        }

    private:

#ifndef SNOWFLAKE_ID_WORKER_NO_LOCK
        std::mutex mutex;
#endif

        /**
            * 开始时间截 (2018-01-01 00:00:00.000)
            */
        const UInt64 twepoch = 1514736000000;
        
        /**
            * 时间戳所占的位数
            */
        const UInt timestampBits = 37;

        /**
            * 机器id所占的位数
            */
        const UInt workerIdBits = 6;

        /**
            * camera id所占的位数
            */
        const UInt camIdBits = 8;

        /**
            * 序列所占的位数
            */
        const UInt sequenceBits = 12;

        /**
            * camera id向左移12位
            */
        const UInt camIdShift = sequenceBits;
        /**
            * 机器ID向左移20位
            */
        const UInt workerIdShift = camIdShift + camIdBits;


        /**
            * 时间截向左移26位
            */
        const UInt timestampLeftShift = workerIdShift + workerIdBits;

        /**
            * 支持的最大机器id，结果是63
            */
        const UInt maxWorkerId = -1 ^ (-1 << workerIdBits);

        /**
            * 支持的最大camera id，结果是255
            */
        const UInt maxCamId = -1 ^ (-1 << camIdBits);

        /**
            * 生成序列的掩码，这里为4095
            */
        const UInt sequenceMask = -1 ^ (-1 << sequenceBits);

        /**
            * 工作机器id(0~31)
            */
        UInt workerId;

        /**
            * camera id(0~255)
            */
        UInt camId;

        /**
            * 毫秒内序列(0~4095)
            */
        AtomicUInt sequence{ 0 };

        /**
            * 上次生成ID的时间截
            */
        AtomicUInt64 lastTimestamp{ 0 };

    };

    typedef SnowflakeIdWorker IdWorker;
}

#endif // _JW_CORE_ID_WORKER_H_
