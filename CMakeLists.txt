cmake_minimum_required (VERSION 2.8.1)
project (ChatServer)

add_definitions(-std=c++11)

add_executable(chatd main.cpp ClientHandler.cpp ChatManager.cpp)

target_link_libraries(chatd pthread)

configure_file(${PROJECT_SOURCE_DIR}/chatd.sh ${PROJECT_BINARY_DIR}/chat)

# Install the binary in /usr/sbin
install(TARGETS chatd DESTINATION /usr/sbin/)

# Install the init script in /etc/init.d/
install(
	FILES ${PROJECT_BINARY_DIR}/chat 
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	DESTINATION /etc/init.d/
)
