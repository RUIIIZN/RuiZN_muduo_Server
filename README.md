RuiZN_Server/
├── README.md
├── base/                    # 基础模块
│   ├── thread/              # 线程相关 (Thread, ThreadPool)
│   ├── time/                # 时间工具 (Timestamp)
│   ├── sync/                # 同步原语 (Mutex, Condition, CountDownLatch)
│   ├── log/                 # 日志系统 (LogStream, Logger)
│   ├── util/                # 通用工具 (StringPiece, Types)
│   └── memory/              # 内存管理 (Atomic, shared_ptr 封装)
├── net/                     # 网络模块
│   ├── channel/             # 事件通道 (Channel)
│   ├── poller/              # IO多路复用 (Poller, EPollPoller)
│   ├── timer/               # 定时器 (TimerQueue, Timer)
│   ├── acceptor/            # 连接接受器 (Acceptor)
│   ├── connector/           # 连接器 (Connector)
│   ├── tcp/                 # TCP协议 (TcpServer, TcpClient, TcpConnection)
│   ├── udp/                 # UDP协议 (UdpServer, UdpClient)
│   ├── http/                # HTTP支持 (HttpServer, HttpResponse)
│   ├── protobuf/            # Protobuf支持
│   └── ssl/                 # SSL/TLS支持
├── examples/                # 示例程序
│   ├── echo/                # Echo服务器/客户端
│   ├── chat/                # 聊天室示例
│   ├── http/                # HTTP服务器示例
│   ├── protobuf/            # Protobuf示例
│   ├── fileserver/          # 文件服务器
│   └── dns/                 # DNS解析示例
├── test/                    # 单元测试
└── build/                   # 编译输出目录