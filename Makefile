CXX = clang++
CXXFLAGS = -std=c++17 -I/opt/homebrew/opt/readline/include
LDFLAGS = -L/opt/homebrew/opt/readline/lib -lreadline

shell: src/shell.cpp src/visuals.cpp
	$(CXX) $(CXXFLAGS) src/shell.cpp src/visuals.cpp $(LDFLAGS) -o shell

run: shell
	./shell

clean:
	rm -f shell