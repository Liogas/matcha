# cpp-template

> Une phrase : à quoi sert cette bibliothèque.

## Exemple minimal

```cpp
#include <tmpl/Version.hpp>

int main()
{
    return tmpl::version().empty() ? 1 : 0;
}
```

## Utilisation

Dans le monorepo (`backend/libs/CMakeLists.txt`) :

```cmake
add_subdirectory(cpp-template)
```

Depuis un autre projet, une fois installée :

```cmake
find_package(cpp-template 0.1 CONFIG REQUIRED)
target_link_libraries(mon_app PRIVATE cpp-template::cpp-template)
```

Option : `CPP_TEMPLATE_BUILD_TESTS` (ON quand la lib est le projet principal, OFF sinon).

## Garanties

- Ce que la bibliothèque promet (thread-safety, exceptions, complexité…).

## Limites

- Ce qu'elle ne fait pas, volontairement.