sudo docker run -v "$(pwd):/usr/src/myapp" --net test-network  -p 8080:8080 --name spring-boot1 spring-boot
