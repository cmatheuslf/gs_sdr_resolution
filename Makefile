CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -IClass -Iutils

# Na Raspberry Pi, basta passar -liio que o sistema localiza a biblioteca automaticamente
LDFLAGS  := -liio -lpthread

TARGET   := sdr_app
SRCS     := gs_executioner.cpp Class/SDR_handler.cpp Class/BPSK_engine.cpp utils/utils.cpp
OBJS     := gs_executioner.o Class/SDR_handler.o Class/BPSK_engine.o utils/utils.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

Class/%.o: Class/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

utils/%.o: utils/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o Class/*.o utils/*.o

re: clean all

.PHONY: all clean re
