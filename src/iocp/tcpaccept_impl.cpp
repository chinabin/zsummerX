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


#include <zsummerX/iocp/iocp_impl.h>
#include <zsummerX/iocp/tcpaccept_impl.h>
using namespace zsummer::network;

TcpAccept::TcpAccept()
{
    //listen
    memset(&_handle._overlapped, 0, sizeof(_handle._overlapped));
    _server = INVALID_SOCKET;
    
    _handle._type = ExtendHandle::HANDLE_ACCEPT;

    //client
    _socket = INVALID_SOCKET;
    memset(_recvBuf, 0, sizeof(_recvBuf));
    _recvLen = 0;
}
TcpAccept::~TcpAccept()
{
    if (_server != INVALID_SOCKET)
    {
        closesocket(_server);
        _server = INVALID_SOCKET;
    }
    if (_socket != INVALID_SOCKET)
    {
        closesocket(_socket);
        _socket = INVALID_SOCKET;
    }
}

std::string TcpAccept::logSection()
{
    std::stringstream os;
    os << " logSecion[0x" << this << "] " << "_summer=" << _summer.use_count() << ", _server=" << _server << ",_ip=" << _ip << ", _port=" << _port <<", _onAcceptHandler=" << (bool)_onAcceptHandler;
    return os.str();
}
bool TcpAccept::initialize(EventLoopPtr& summer)
{
    if (_summer)
    {
        LCF("TcpAccept already initialize! " << logSection());
        return false;
    }
    _summer = summer;
    return true;
}

/*
设置服务器监听套接字并绑定完成端口
*/
bool TcpAccept::openAccept(const std::string ip, unsigned short port , bool reuse )
{
    if (!_summer)
    {
        LCF("TcpAccept not inilialize!  ip=" << ip << ", port=" << port << logSection());
        return false;
    }

    if (_server != INVALID_SOCKET)
    {
        LCF("TcpAccept already opened!  ip=" << ip << ", port=" << port << logSection());
        return false;
    }
    _ip = ip;
    _port = port;
    _isIPV6 = _ip.find(':') != std::string::npos;
    if (_isIPV6)
    {
        _server = WSASocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        setIPV6Only(_server, false);
    }
    else
    {
        _server = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    }
    if (_server == INVALID_SOCKET)
    {
        LCF("create socket error! ");
        return false;
    }


    if (reuse)
    {
        setReuse(_server);
    }
    
    if (_isIPV6)
    {
        SOCKADDR_IN6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        if (ip.empty() || ip == "::")
        {
            addr.sin6_addr = in6addr_any;
        }
        else
        {
            auto ret = inet_pton(AF_INET6, ip.c_str(), &addr.sin6_addr);
            if (ret <= 0)
            {
                LCF("bind ipv6 error, ipv6 format error" << ip);
                closesocket(_server);
                _server = INVALID_SOCKET;
                return false;
            }
        }
        addr.sin6_port = htons(port);
        auto ret = bind(_server, (sockaddr *)&addr, sizeof(addr));
        if (ret != 0)
        {
            LCF("bind ipv6 error, ERRCODE=" << WSAGetLastError());
            closesocket(_server);
            _server = INVALID_SOCKET;
            return false;
        }
    }
    else
    {
        SOCKADDR_IN addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ip.empty() ? INADDR_ANY : inet_addr(ip.c_str());
        addr.sin_port = htons(port);
        if (bind(_server, (sockaddr *)&addr, sizeof(addr)) != 0)
        {
            LCF("bind error, ERRCODE=" << WSAGetLastError());
            closesocket(_server);
            _server = INVALID_SOCKET;
            return false;
        }
        
    }
	if (true)
	{
		int OptionValue = 1;
		DWORD NumberOfBytesReturned = 0;
		DWORD SIO_LOOPBACK_FAST_PATH_A = 0x98000010;

		WSAIoctl(
			_server,
			SIO_LOOPBACK_FAST_PATH_A,		// 提升本地回环网络性能 https://docs.microsoft.com/en-us/previous-versions/windows/desktop/legacy/jj841212(v=vs.85)?redirectedfrom=MSDN
			&OptionValue,
			sizeof(OptionValue),
			NULL,
			0,
			&NumberOfBytesReturned,
			0,
			0);
	}

    if (listen(_server, SOMAXCONN) != 0)
    {
        LCF("listen error, ERRCODE=" << WSAGetLastError() );
        closesocket(_server);
        _server = INVALID_SOCKET;
        return false;
    }

    if (CreateIoCompletionPort((HANDLE)_server, _summer->_io, (ULONG_PTR)this, 1) == NULL)
    {
        LCF("bind to iocp error, ERRCODE=" << WSAGetLastError() );
        closesocket(_server);
        _server = INVALID_SOCKET;
        return false;
    }
    return true;
}

bool TcpAccept::close()
{
    if (_server != INVALID_SOCKET)
    {
        LCI("TcpAccept::close. socket=" << _server);
        closesocket(_server);
        _server = INVALID_SOCKET;
    }
    return true;
}

/*
投递监听请求到 IOCP。
传入 TcpSocketPtr 接管客户端接入后的 IO 操作，以及 handler 作为客户端接入成功后的回调。
设置对应的重叠结构(_handle)数据。
*/
bool TcpAccept::doAccept(const TcpSocketPtr & s, _OnAcceptHandler&& handler)
{
	// 为什么：这个函数每次调用都会做好客户端接入的准备，怎么知道第二次准备的调用时机？
	// 虽然 onIOCPMessage 函数会对 _onAcceptHandler 调用 move ，但是外界咋知道我能调用 doAccept 了，不会因为下面这个 if 而失败。
    if (_onAcceptHandler)
    {
        LCF("duplicate operation error." << logSection());
        return false;
    }
    if (!_summer || _server == INVALID_SOCKET)
    {
        LCF("TcpAccept not initialize." << logSection());
        return false;
    }
    
    _client = s;
    _socket = INVALID_SOCKET;
    memset(_recvBuf, 0, sizeof(_recvBuf));
    _recvLen = 0;
    unsigned int addrLen = 0;
    if (_isIPV6)
    {
        addrLen = sizeof(SOCKADDR_IN6)+16;
        _socket = WSASocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    }
    else
    {
        addrLen = sizeof(SOCKADDR_IN)+16;
        _socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    }
    if (_socket == INVALID_SOCKET)
    {
        LCF("TcpAccept::doAccept create client socket error! error code=" << WSAGetLastError() );
        return false;
    }
    setNoDelay(_socket);
	/*
	投递异步连接请求：
	函数：AcceptEx
	参数：
	一参本地监听Socket
	二参为即将到来的客人准备好的Socket
	三参接收缓冲区：
		一存客人发来的第一份数据、二存Server本地地址、三存Client远端地址。地址包括IP和端口。
	四参定三参数据区长度，0表只连不接收数据（但是上面说的本地和远端地址还是会有）
	五参定三参本地地址区长度，至少sizeof(sockaddr_in) + 16
	六参定三参远端地址区长度，至少sizeof(sockaddr_in) + 16
	七参表示接收到的数据长度
	八参不用管
	*/
    if (!AcceptEx(_server, _socket, _recvBuf, 0, addrLen, addrLen, &_recvLen, &_handle._overlapped))
    {
        if (WSAGetLastError() != ERROR_IO_PENDING)
        {
            LCE("TcpAccept::doAccept do AcceptEx error, error code =" << WSAGetLastError() );
            closesocket(_socket);
            _socket = INVALID_SOCKET;
            return false;
        }
    }
    _onAcceptHandler = handler;
    _handle._tcpAccept = shared_from_this();
    return true;
}

/*
处理 IOCP 关于客户端接入消息。
	保存接入的客户端 ip 、端口、套接字、是否是 ipv6 信息。
	调用 doAccept 中设置的回调接口。
*/
bool TcpAccept::onIOCPMessage(BOOL bSuccess)
{
    std::shared_ptr<TcpAccept> guard( std::move(_handle._tcpAccept));		// 为什么？
    _OnAcceptHandler onAccept(std::move(_onAcceptHandler));
    if (bSuccess)
    {
		// 继承 _server 的属性。调用 AcceptEx 成功接入客户端之后规定必须这样。
        if (setsockopt(_socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&_server, sizeof(_server)) != 0)
        {
            LCW("setsockopt SO_UPDATE_ACCEPT_CONTEXT fail!  last error=" << WSAGetLastError() );
        }
        if (_isIPV6)
        {
            sockaddr * paddr1 = NULL;
            sockaddr * paddr2 = NULL;
            int tmp1 = 0;
            int tmp2 = 0;
            GetAcceptExSockaddrs(_recvBuf, _recvLen, sizeof(SOCKADDR_IN6) + 16, sizeof(SOCKADDR_IN6) + 16, &paddr1, &tmp1, &paddr2, &tmp2);
            char ip[50] = { 0 };
            inet_ntop(AF_INET6, &(((SOCKADDR_IN6*)paddr2)->sin6_addr), ip, sizeof(ip));
            _client->attachSocket(_socket, ip, ntohs(((sockaddr_in6*)paddr2)->sin6_port), _isIPV6);
            onAccept(NEC_SUCCESS, _client);
        }
        else
        {
			// 获取接入的客户端信息并保存，最后调用回调函数
            sockaddr * paddr1 = NULL;
            sockaddr * paddr2 = NULL;
            int tmp1 = 0;
            int tmp2 = 0;
			/*
			用来解析 AcceptEx 接收到的地址数据
			一和二参都是拿 AcceptEx 参数
			三和四参是规定这么大
			五和六参表示本地地址及其长度
			七和八参表示远端地址及其长度
			*/
            GetAcceptExSockaddrs(_recvBuf, _recvLen, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &paddr1, &tmp1, &paddr2, &tmp2);
            _client->attachSocket(_socket, inet_ntoa(((sockaddr_in*)paddr2)->sin_addr), ntohs(((sockaddr_in*)paddr2)->sin_port), _isIPV6);
            onAccept(NEC_SUCCESS, _client);
        }

    }
    else
    {
        closesocket(_socket);
        _socket = INVALID_SOCKET;
        LCW("AcceptEx failed ... , lastError=" << GetLastError() );
        onAccept(NEC_ERROR, _client);
    }
    return true;
}
