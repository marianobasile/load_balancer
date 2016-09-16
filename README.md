# BALANCER
Description:  Datapipe, a simple TCP/IP socket redirection application, has been extended for the purpose of implementing
              a workload balancer.
              
# Build
Make

#Usage
./datapipe LOCAL_ADDR LOCAL_PORT SERVER_ADDR1 SERVER_PORT1 SERVER_ADDRN SERVER_PORTN

#Example
./datapipe 127.0.0.1 1233 127.0.0.1 1234 127.0.0.1 1235 127.0.0.1 1236
