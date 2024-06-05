REDIS_SERVER_NAME=redis-server
REDIS_SERVER_OBJ=server.o db.o dict.o networking.o sds.o t_string.o zmalloc.o tmp.o object.o debug.o

%.o: %.c
	$(CC) -MMD -g -o $@ -c $<


# redis-server
$(REDIS_SERVER_NAME): $(REDIS_SERVER_OBJ)
	rm -rf $(REDIS_SERVER_NAME)
	$(CC) -o $@ $^

test: $(REDIS_SERVER_NAME)
	@(cd ..; ./runtest)

clean:
	rm -rf $(REDIS_SERVER_NAME) *.o *.d
	
all: $(REDIS_SERVER_NAME)
	@echo ""
	@echo "Hint: It's a good idea to run 'make test' ;)"
	@echo ""
