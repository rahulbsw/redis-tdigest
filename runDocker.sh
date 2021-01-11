docker stop redis-tdigest
docker rm redis-tdigest
make -f DockerMakefile  >> dockerMake.log
tail -1 dockerMake.log 
export image_name=`tail -1 dockerMake.log | grep -oE '[^ ]+$'`
echo 'Starting redis-tdigest image $image_name'
docker run -d -p 6379:6379 --name redis-tdigest $image_name
docker logs -f  redis-tdigest
tail -1