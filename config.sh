# root soft nofile 32768
# root hard nofile 32768
# frank soft nofile 32768
# frank hard nofile 32768
# root soft nproc 204800
# root hard nproc 204800
# frank soft nproc 204800
# frank hard nproc 204800
# https://julyclyde.org/?p=578
# https://www.cnblogs.com/ymei/p/11776582.html
# https://zhuanlan.zhihu.com/p/500404323
# relogin
# su frank

# see current prlimits
# cat /proc/self/limits | grep file

# # current pid
# cat /proc/self/limits

# default config: 8GB RAM, 8 virtual processors
# 8: agent reactors for dealing with differenct tcp connecetions
# 4: LogLevel TRACE, DEBUG, INFO, WARN, ERROR, FATAL, NUM_LOG_LEVELS,
# 0: Using Zerocopy
# 8: 8KB for zerocopy

# https://www.cnblogs.com/SocialistYouth/p/16691035.html
# Windows Defender
for pid in $(sudo lsof -t -i:8000); do sudo kill -9 $pid; done
# ./build/bin/HttpServer_test 8 0 0 8
# ./build/bin/HttpServer_test 8 3 0 8

./build/bin/HttpServer_test 8 3 0 1
# ./test_presure/webbench-1.5/webbench http://localhost:8000/ -c 10000 -t 20 Webbench - Simple Web Benchmark 1.5 Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software. Benchmarking: GET http://localhost:8000/ 10000 clients, running 20 sec. Speed=1466346 pages/min, 12317382 bytes/sec. Requests: 488782 susceed, 0 failed.