REDIS_SERVER_NAME=redis-server
REDIS_SERVER_OBJ=server.o

%.o: %.c
	$(CC) -MMD -o $@ -c $<


# redis-server
$(REDIS_SERVER_NAME): $(REDIS_SERVER_OBJ)
	$(CC) -o $@ $^

test: $(REDIS_SERVER_NAME)
	@(cd ..; ./runtest)

clean:
	rm -rf $(REDIS_SERVER_NAME) *.o *.d
	
all: $(REDIS_SERVER_NAME)
	@echo ""
	@echo "Hint: It's a good idea to run 'make test' ;)"
	@echo ""
