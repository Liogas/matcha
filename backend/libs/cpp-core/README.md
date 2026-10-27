# cpp-core

> Briques communes aux bibliothèques : erreurs typées (`Result<T>`), lecture de la
> configuration et des secrets, logs structurés.

## Exemple minimal

```cpp
#include <core/ConfigReader.hpp>
#include <core/Logger.hpp>
#include <iostream>

int main()
{
    core::StreamLogger logger(std::cerr, core::LogLevel::Info);
    const core::SystemEnvironment environment;
    core::ConfigReader config(environment);

    const std::string host = config.required("DB_HOST");
    const int port = config.requiredInt("DB_PORT");
    const std::string password = config.requiredSecret("DB_PASSWORD"); // ou DB_PASSWORD_FILE

    if (const auto checked = config.finish(); !checked)
    {
        logger.error("Invalid configuration", {{"errors", checked.error().message}});
        return 1;
    }
    logger.info("Configuration loaded", {{"host", host}, {"password", password}});
    // level=INFO msg="Configuration loaded" host=db password=***
}
```

## Utilisation

Dans le monorepo (`backend/libs/CMakeLists.txt`), avant les libs qui en dépendent :

```cmake
add_subdirectory(cpp-core)
```

Depuis un autre projet, une fois installée :

```cmake
find_package(cpp-core 0.1 CONFIG REQUIRED)
target_link_libraries(mon_app PRIVATE cpp-core::cpp-core)
```

Option : `CPP_CORE_BUILD_TESTS` (ON quand la lib est le projet principal, OFF sinon).
Nécessite C++23 (`std::expected`).

## Garanties

- `Result<T>` est un alias de `std::expected<T, core::Error>` : aucune exception pour les
  erreurs attendues.
- `ConfigReader` ne lève pas d'exception et rapporte toutes les erreurs d'un coup dans
  `finish()`. Une variable vide est traitée comme absente.
- `StreamLogger` est utilisable depuis plusieurs threads : une ligne n'est jamais coupée.
  Les champs dont la clé contient `password`, `token` ou `secret` sont masqués, et les
  retours à la ligne sont échappés.

## Limites

- Pas d'horodatage dans les logs (ajouté par Docker), pas de sortie JSON.
- `ConfigReader` ne lit que les types utilisés par le backend : chaîne, `int`, booléen
  (`true`/`false` uniquement), secret.
- Le masquage se fait sur le **nom** du champ : une valeur sensible sous un nom anodin
  n'est pas détectée.