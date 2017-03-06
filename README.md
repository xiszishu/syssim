# syssim
both SPS and Hashtable benchmarks are implemented in the syssim.cc as subthread
sps.txt and hashtable.txt are inputs files for these two benchmark, two numbers represents the size and operations
To choose each benchmark to run comment or uncomment:
pthread_create(&thread[0],NULL,sps,p);
pthread_create(&thread[0],NULL,hashtable,p);

Currently still under development.
