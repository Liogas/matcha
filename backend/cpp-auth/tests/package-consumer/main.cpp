#include <auth/session/InMemorySessionStore.hpp>
#include <auth/session/SessionManager.hpp>

#include <chrono>

int main()
{
    auth::InMemorySessionStore store;
    auth::SessionManager sessions(store);
    const auto now = std::chrono::system_clock::now();

    if (!sessions.createSession(
            "package-consumer-token",
            "package-consumer-user",
            std::chrono::minutes{1},
            now
        ))
        return 1;

    return sessions.isValid("package-consumer-token", now) ? 0 : 1;
}
