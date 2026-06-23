NAME     = webserv
CXX      = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
SRCS = main.cpp Server.cpp ServerRouting.cpp ServerHandlers.cpp Config.cpp HTTPRequest.cpp HTTPResponse.cpp
OBJS     = $(SRCS:.cpp=.o)
HEADERS  = Server.hpp Config.hpp HTTPRequest.hpp HTTPResponse.hpp

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
