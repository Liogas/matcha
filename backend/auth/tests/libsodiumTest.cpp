#include <sodium.h>

#include <iostream>
#include <cstring>

int main()
{
    if (sodium_init() < 0)
    {
        std::cerr << "[ERROR LIBSODIUM] initialisation failed" << std::endl;
        return (1);
    }

    const char *password = "my_password";
    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(
        hash,
        password,
        std::strlen(password),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE
    ) != 0)
    {
        std::cerr << "[ERROR LIBSODIUM] password hashing failed" << std::endl;
        return (1);
    }
    std::cout << "[LOG LIBSODIUM] hash -> " << hash << std::endl;
    return (0);
}