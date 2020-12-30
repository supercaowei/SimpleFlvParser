LIBS := ./libs
TMP := ./tmp

all: flvparser

# JSON_DIR := ../third_party/jsoncpp
# JSON_INCLUDE_DIR := $(JSON_DIR)/include
# JSON_SRC_DIR := $(JSON_DIR)/src/lib_json
# JSON_FILES := $(JSON_SRC_DIR)/json_reader.cpp $(JSON_SRC_DIR)/json_value.cpp $(JSON_SRC_DIR)/json_writer.cpp 
# JSON_TMP := ./tmp/jsoncpp
# JSON_OBJS := $(JSON_TMP)/json_reader.o $(JSON_TMP)/json_value.o $(JSON_TMP)/json_writer.o

# jsoncpp: $(JSON_FILES)
# 	cc -I $(JSON_INCLUDE_DIR) -x c++ -std=c++11 -I $(JSON_SRC_DIR) -c $(JSON_FILES)
# 	mkdir -p $(JSON_TMP)
# 	mv -f *.o $(JSON_TMP)
# 	ar -r libjson.a $(JSON_OBJS)
# 	mkdir -p $(LIBS)
# 	mv -f libjson.a $(LIBS)


# SQLITE_NAME=sqlite3
# SQLITE_DIR := ../third_party/sqlite
# SQLITE_TMP := ./tmp/sqlite

# $(SQLITE_NAME): $(SQLITE_DIR)/$(SQLITE_NAME).c
# 	cc -I $(SQLITE_DIR) -x c -c $(SQLITE_DIR)/$(SQLITE_NAME).c
# 	mkdir -p $(SQLITE_TMP)
# 	mv -f $(SQLITE_NAME).o $(SQLITE_TMP)
# 	ar -r lib$(SQLITE_NAME).a $(SQLITE_TMP)/$(SQLITE_NAME).o
# 	mkdir -p $(LIBS)
# 	mv -f lib$(SQLITE_NAME).a $(LIBS)

FLV_PARSER_DIR := ../SimpleFlvParser
JSON_INCLUDE_DIR := ../third_party/jsoncpp/include
SQLITE_DIR := ../third_party/sqlite
FLV_PARSER_TMP := ./tmp/SimpleFlvParser

# flvparser: jsoncpp sqlite3
flvparser:
	cc -I -Wall -g -c $(FLV_PARSER_DIR)/*.c 
	cc -I $(FLV_PARSER_DIR) -I $(JSON_INCLUDE_DIR) -I $(SQLITE_DIR) -Wall -std=c++11 -c -g $(FLV_PARSER_DIR)/*.cpp
	mkdir -p $(FLV_PARSER_TMP)
	mv -f *.o $(FLV_PARSER_TMP)
	cc -g $(FLV_PARSER_TMP)/*.o -L$(LIBS) -ljson -lsqlite3 -lc++ -o SimpleFlvParser
	mkdir -p ./bin
	mv -f SimpleFlvParser ./bin

clean:
	# rm -fr libs
	rm -fr tmp
	rm -fr bin
