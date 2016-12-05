gcc -o test test.cpp  ../libzkclient.a -I/usr/local/include/zookeeper -I../  -I../inc -lboost_thread -lboost_date_time -lzookeeper_mt -lpthread 
gcc -o zktest zktest.cpp  ../libzkclient.a -I/usr/local/include/zookeeper -I../  -I../inc -lboost_thread -lboost_date_time -lzookeeper_mt -lpthread 
gcc -o zktest2 zktest2.cpp  ../libzkclient.a -I/usr/local/include/zookeeper -I../  -I../inc -lboost_thread -lboost_date_time -lzookeeper_mt -lpthread 
