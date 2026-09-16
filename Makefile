CXX = clang++
CXXFLAGS = -std=c++17 -I/opt/homebrew/opt/readline/include
LDFLAGS = -L/opt/homebrew/opt/readline/lib -lreadline

shell: shell.cpp visuals.cpp
	$(CXX) $(CXXFLAGS) shell.cpp visuals.cpp $(LDFLAGS) -o shell

run: shell
	./shell

clean:
	rm -f shell