/*
 * zsummerX License
 * -----------
 *
 * zsummerX is licensed under the terms of the MIT license reproduced below.
 * This means that zsummerX is free software and can be used for both academic
 * and commercial purposes at absolutely no cost.
 *
 *
 * ===============================================================================
 *
 * Copyright (C) 2010-2015 YaweiZhang <yawei.zhang@foxmail.com>.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * ===============================================================================
 *
 * (end of COPYRIGHT)
 */


//! common header 

#ifndef ZSUMMER_CONFIG_H_
#define ZSUMMER_CONFIG_H_

#include <iostream>
#include <queue>
#include <iomanip>
#include <string.h>
#include <signal.h>
#include <unordered_map>
#include <log4z/log4z.h>
#include <proto4z/proto4z.h>

#ifdef WIN32
#pragma warning(disable:4503)
#pragma warning(disable:4200)
#include "../iocp/iocp_impl.h"
#include "../iocp/tcpsocket_impl.h"
#include "../iocp/udpsocket_impl.h"
#include "../iocp/tcpaccept_impl.h"
#elif defined(__APPLE__) || defined(__SELECT__)
#include "../select/select_impl.h"
#include "../select/udpsocket_impl.h"
#include "../select/tcpsocket_impl.h"
#include "../select/tcpaccept_impl.h"
#else
#include "../epoll/epoll_impl.h"
#include "../epoll/tcpsocket_impl.h"
#include "../epoll/udpsocket_impl.h"
#include "../epoll/tcpaccept_impl.h"
#endif


namespace zsummer
{
    namespace network
    {
        //! define network type
        using SessionID = unsigned int;
        const SessionID InvalidSessionID = 0;
        using AccepterID = unsigned int;
        const AccepterID InvalidAccepterID = 0;


        //! define session id range.
        const unsigned int __MIDDLE_SEGMENT_VALUE = 300 * 1000 * 1000;
		// 为什么？SessionID 和 ConnectID 的检测会有问题，unsigned int标准规定的长度是至少16位，也就是最大65535。
        //(InvalidSessionID - __MIDDLE_SEGMENT_VALUE)
        inline bool isSessionID(unsigned int unknowID){ return unknowID != InvalidSessionID && unknowID < __MIDDLE_SEGMENT_VALUE; }
        //(InvalidSessionID - power(2, 32))
        inline bool isConnectID(unsigned int unknowID){ return unknowID != InvalidSessionID && unknowID >= __MIDDLE_SEGMENT_VALUE; }
        inline SessionID nextSessionID(SessionID curSessionID){ return curSessionID + 1 >= __MIDDLE_SEGMENT_VALUE ? 1 : curSessionID + 1; }
        inline SessionID nextConnectID(SessionID curSessionID){ return curSessionID + 1 <= __MIDDLE_SEGMENT_VALUE ? __MIDDLE_SEGMENT_VALUE +1 : curSessionID + 1; }
        inline AccepterID nextAccepterID(AccepterID AccepterID){ return AccepterID + 1 >= __MIDDLE_SEGMENT_VALUE ? 1 : AccepterID + 1; }


        const unsigned int SESSION_BLOCK_SIZE = 20 * 1024;

        enum ProtoType
        {
            PT_TCP,
            PT_HTTP,
        };

        enum BLOCK_CHECK_TYPE
        {
            BCT_SUCCESS = 0, //success, return block size 
            BCT_SHORTAGE = 1, //too short, return need size 
            BCT_CORRUPTION = 2, //error, need close socket. 
        };

        enum StatType
        {
            STAT_STARTTIME,
            STAT_SESSION_CREATED,
            STAT_SESSION_DESTROYED,
            STAT_SESSION_LINKED,
            STAT_SESSION_CLOSED,
            STAT_FREE_BLOCKS,
            STAT_EXIST_BLOCKS,
            STAT_SEND_COUNT,
            STAT_SEND_PACKS,
            STAT_SEND_BYTES,
            STAT_SEND_QUES,
            STAT_RECV_COUNT,
            STAT_RECV_PACKS,
            STAT_RECV_BYTES,
            
            STAT_SIZE,
        };
        const char * const StatTypeDesc[] = { 
            "STAT_STARTTIME",
            "STAT_SESSION_CREATED",
            "STAT_SESSION_DESTROYED",
            "STAT_SESSION_LINKED",
            "STAT_SESSION_CLOSED",
            "STAT_FREE_BLOCKS",
            "STAT_EXIST_BLOCKS",
            "STAT_SEND_COUNT",
            "STAT_SEND_PACKS",
            "STAT_SEND_BYTES",
            "STAT_SEND_QUES",
            "STAT_RECV_COUNT",
            "STAT_RECV_PACKS",
            "STAT_RECV_BYTES",
            "STAT_SIZE", };

        class TcpSession;
        using TcpSessionPtr = std::shared_ptr<TcpSession>;

        struct SessionBlock 
        {
            unsigned int type = 0;
            unsigned int createTime = 0;
            unsigned int reused = 0;
            unsigned int timestamp = 0;
            unsigned int timetick = 0;
            unsigned int bound = 0;			// 单次数据发送/接收的字节最大数
            unsigned int len = 0;			// 待发送/已接收处理的数据字节数
            char begin[0];					// 指向待发送/已接收待处理的数据块
        };
        

        using CreateBlock = std::function<SessionBlock * ()>;

        using FreeBlock = std::function<void(SessionBlock *)>;
        
        using OnBlockCheckResult = std::pair<BLOCK_CHECK_TYPE, unsigned int>;

        //check one block integrity 
        using OnBlockCheck = std::function<OnBlockCheckResult(const char * /*begin*/, unsigned int /*len*/, unsigned int /*bound*/, unsigned int /*blockLimit*/)>;

        //!dispatch one integrity block 
        using OnBlockDispatch = std::function<void(const TcpSessionPtr &  /*session*/, const char * /*begin*/, unsigned int /*len*/)>;

        //!event linked, closed, ontimer
        using OnSessionEvent = std::function<void(const TcpSessionPtr &  /*session*/)>;

 
        //!HTTP unpack, hadHeader used by 'chunked', commonLine can be GET, POST RESPONSE.  
        using OnHTTPBlockCheck = std::function<OnBlockCheckResult(const char * /*begin*/, unsigned int /*len*/, unsigned int /*bound*/,
            bool & /*isChunked*/, std::string& /*method*/, std::string &methodLine/*method line*/, std::map<std::string,std::string> & /*head*/, std::string & /*body*/)>;
        //!HTTP dispatch  
        using OnHTTPBlockDispatch = std::function<
            void(TcpSessionPtr /*session*/, const std::string& /*method*/, const std::string &methodLine/*method line*/ , const std::map<std::string, std::string> &/*head*/, const std::string & /*body*/)>;
        


        struct SessionOptions 
        {

            ProtoType       _protoType = PT_TCP;
            std::string     _rc4TcpEncryption = ""; //empty is not encryption 
            bool            _openFlashPolicy = false; //check falsh client  
            bool            _setNoDelay = true;
            bool            _floodSendOptimize = true;
            bool            _joinSmallBlock = true; //merge small block  
            unsigned int    _sessionPulseInterval = 30000;  // 连接成功的心跳频率
            unsigned int    _connectPulseInterval = 5000;	// 连接的心跳频率
            unsigned int    _reconnects = 0; // can reconnect count 
            bool            _reconnectClean = true;//clean unsend block . 
            unsigned int    _maxSendListCount = 600;		// 发送队列最大长度
            OnBlockCheck _onBlockCheck;						// 收到数据之后的回调函数，可以用来检查数据是否对等。
            OnBlockDispatch _onBlockDispatch;				// 检查过后的数据分发接口
            OnHTTPBlockCheck _onHTTPBlockCheck;
            OnHTTPBlockDispatch _onHTTPBlockDispatch;
            OnSessionEvent _onReconnectEnd;			// 到达重连次数上限的时候的回调函数
            OnSessionEvent _onSessionClosed;		// 关闭连接的回调函数
            OnSessionEvent _onSessionLinked;		// 连接服务器成功的回调函数
            OnSessionEvent _onSessionPulse;			// 连接成功之后的心跳回调函数

            CreateBlock _createBlock ;
            FreeBlock _freeBlock;

        };

        struct AccepterOptions
        {
            AccepterID _aID = InvalidAccepterID;
            TcpAcceptPtr _accepter;
            std::string        _listenIP;
            unsigned short    _listenPort = 0;
            bool            _setReuse = true;
            unsigned int    _maxSessions = 5000;
            std::vector<std::string> _whitelistIP;
            unsigned long long _totalAcceptCount = 0;
            unsigned long long _currentLinked = 0;
            bool _closed = false;
            SessionOptions _sessionOptions;
        };

		/*
		第一个参数表示这个元祖是否已经初始化
		*/
        using TupleParam = std::tuple<bool, double, unsigned long long, std::string>;

        enum TupleParamType
        {
            TupleParamInited = 0,
            TupleParamNumber = 1,
            TupleParamInteger = 2,
            TupleParamString = 3,
        };

        inline zsummer::log4z::Log4zStream & operator << (zsummer::log4z::Log4zStream &os, const SessionOptions & traits)
        {
            os << "{ " << "_protoType=" << traits._protoType
                << ", _rc4TcpEncryption=" << traits._rc4TcpEncryption
                << ", _openFlashPolicy=" << traits._openFlashPolicy
                << ", _setNoDelay=" << traits._setNoDelay
                << ", _sessionPulseInterval=" << traits._sessionPulseInterval
                << ", _connectPulseInterval=" << traits._connectPulseInterval
                << ", _reconnects=" << traits._reconnects
                << ", _reconnectClean=" << traits._reconnectClean
                << "}";
            return os;
        }


        inline zsummer::log4z::Log4zStream & operator << (zsummer::log4z::Log4zStream &os, const AccepterOptions & extend)
        {
            os << "{"
                << "_aID=" << extend._aID
                << ", _listenIP=" << extend._listenIP
                << ", _listenPort=" << extend._listenPort
                << ", _maxSessions=" << extend._maxSessions
                << ",_totalAcceptCount=" << extend._totalAcceptCount
                << ", _currentLinked=" << extend._currentLinked
                << ",_whitelistIP={";

            for (auto str : extend._whitelistIP)
            {
                os << str << ",";
            }
            os << "}";
            os << ", SessionOptions=" << extend._sessionOptions;
            os << "}";
            return os;
        }
    }
}





#endif

