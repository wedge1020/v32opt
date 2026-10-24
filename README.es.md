# v32opt — Optimizador de Ensamblador para Vircon32

[Versión en inglés / in English](README.md) | [Versión en francés / en français](README.fr.md)

`v32opt` es un optimizador de ensamblador modular y de múltiples pasadas,
escrito en C, dirigido específicamente a la consola de fantasía
**Vircon32**. Toma el ensamblador en bruto producido por un compilador para
Vircon32 (C; C++ a través de `v32c++`; y Lua mediante `v32lua`),
ensamblador escrito a mano o (con cuidado, véase más abajo) CARTs de
Vircon32 desensamblados, y aplica optimizaciones iterativas locales,
estructurales, de flujo de datos y de promoción a registros para reducir
el tamaño del código, ver en acción cada uno de los pasos de la cadena de
construcción y mejorar la eficiencia de ejecución.

> **Nota sobre la arquitectura de Vircon32:** En la CPU de Vircon32,
> **todas las instrucciones se ejecutan en exactamente 1 ciclo**, y **no
> existen indicadores (flags) de CPU que reaccionen a las instrucciones**
> (como Carry, Zero u Overflow). Las instrucciones de comparación
> destructivas (p. ej., `IEQ`, `INE`) sobrescriben directamente el registro
> de destino con `1` (verdadero) o `0` (falso), que los saltos condicionales
> (`JT`, `JF`) evalúan directamente. Aunque muchas simplificaciones
> algebraicas son neutras en ciclos debido a este modelo de tiempos plano,
> `v32opt` las aplica de forma agresiva para reducir el tamaño total del
> binario, disminuir la presión sobre los registros, eliminar cuellos de
> botella de memoria e imponer un ensamblador limpio e idiomático.


## Colaboración con IA

Esta herramienta se desarrolló en estrecha colaboración con IA. Dudo en
llamarla "vibe-coded", ya que tengo algo de conocimiento y experiencia con
lo básico (del ensamblador, de la optimización de código). Pero,
ciertamente, no habría alcanzado este nivel de funcionalidad en tan poco
tiempo si la IA no hubiera participado activamente. Siento que asumí más
bien un papel de "jefe de proyecto": marcando la dirección, resolviendo
problemas y depurando donde hacía falta.

Aunque quizá no conozca cada rincón del código tan bien como si lo
hubiera escrito enteramente yo mismo, sí aprecio la ventaja que ofrece
para crear nuevas herramientas que puedo usar en el aula, compartir con la
comunidad y hacer realidad las ideas con mayor rapidez.

Sé que esto puede rozar o cruzar una línea de dudosa ambigüedad o
aceptabilidad para algunos. Pero, siendo una herramienta de nicho en un
entorno de nicho, que sea igual de fácil de ignorar.

---

## Tabla de Contenidos

1. [Instrucciones de Compilación](#instrucciones-de-compilación)
2. [Integración en el proceso de construcción del CART](#integración-en-el-proceso-de-construcción-del-cart)
3. [Lenguajes de Origen: C, C++, Lua y Ensamblador](#lenguajes-de-origen-c-c-lua-y-ensamblador)
4. [Uso y Niveles de Optimización](#uso-y-niveles-de-optimización)
5. [Fase 1: Optimizaciones Locales de Mirilla (-O1)](#fase-1-optimizaciones-locales-de-mirilla--o1)
6. [Fase 2: Flujo de Datos Global y Código Muerto (-O2)](#fase-2-flujo-de-datos-global-y-código-muerto--o2)
7. [Fase 3: Inserción en Línea Interprocedimental (-O3)](#fase-3-inserción-en-línea-interprocedimental--o3)
8. [Promoción de Memoria a Registros (-O2)](#promoción-de-memoria-a-registros--o2)
9. [Visualización del CFG](#visualización-del-cfg)
10. [Salvaguardas de Seguridad y Corrección](#salvaguardas-de-seguridad-y-corrección)
11. [Opciones de Diagnóstico y Depuración](#opciones-de-diagnóstico-y-depuración)
12. [Publicación de versiones (sellado de versión)](#publicación-de-versiones-sellado-de-versión)

---

## Instrucciones de Compilación

`v32opt` está escrito en C estándar y está diseñado para compilarse
fácilmente en cualquier plataforma con un compilador de C moderno (`gcc` o
`clang`). Las cabeceras del proyecto están en `inc/` (separadas por
función: `v32opt.h` es la cabecera general, con `asm.h`, `peephole.h`,
`dataflow.h`, `inline.h`, `stack.h` y `promote.h` detrás de ella); los
fuentes están en `src/` y `src/peephole/`.

Hay dos formas independientes de compilarlo: el `Makefile` base (la
compilación habitual de desarrollo) y CMake (una compilación fuera del
árbol de fuentes con soporte de instalación en el sistema y de
empaquetado, inspirada en la configuración de CMake de las propias DevTools
de Vircon32). Nunca comparten productos de compilación, por lo que ambas
pueden usarse a la vez.

### Con Make (Linux / macOS / MSYS2)

```bash
# Compila el binario (se escribe en bin/v32opt; bin/v32opt.exe en Windows)
$ make

# Instala en tu directorio bin personal (~/bin/bin.<arch> si existe,
# si no ~/bin); "make uninstall" lo vuelve a quitar
$ make install

# O instala en todo el sistema: v32opt en /usr/local/bin y la página de
# manual en /usr/local/share/man/man1 (PREFIX=... cambia la raíz; se
# respeta DESTDIR=... para preparar paquetes). "sudo make sysuninstall"
# quita ambos.
$ sudo make sysinstall

# ...con la página de manual en español (es) o francés (fr) en lugar de
# la inglesa
$ sudo make sysinstall DEST_LANG=es

# Ejecuta la batería de pruebas unitarias (necesita las DevTools de
# Vircon32 -- assemble, packrom -- en tu PATH; la prueba primes necesita
# además v32lua)
$ make tests

# Limpia los productos de compilación ("make distclean" también elimina un
# directorio build/ de CMake)
$ make clean
```

### Con CMake (Linux / macOS / Windows)

CMake compila fuera del árbol de fuentes, en un directorio `build/` (se
rechaza un `cmake .` dentro del árbol, ya que sobrescribiría el `Makefile`
base):

```bash
$ mkdir build && cd build
$ cmake ..                    # compilación Release por defecto
$ cmake --build .
$ ctest                       # pruebas rápidas (no necesitan herramientas de Vircon32)
$ sudo cmake --install .      # véanse las ubicaciones de instalación abajo
```

| Plataforma | Ejecutable `v32opt` | Página de manual / documentación |
| --- | --- | --- |
| Linux, macOS (y otros Unix) | `/usr/local/bin/v32opt` | `/usr/local/share/man/man1/v32opt.1` |
| Windows (MSYS2 + MinGW) | `<Program Files>\Vircon32\v32tools\v32opt.exe` | `<Program Files>\Vircon32\v32tools\docs\v32opt\` (`README.md`, `v32opt.1`) |

En Windows, el optimizador se instala dentro del árbol de instalación
predeterminado de Vircon32, en un directorio `v32tools/`, así que asegúrate
de añadir la entrada correspondiente al `PATH`. Como con las DevTools,
compílalo con MSYS2 + MinGW (Visual C++ no tiene `getopt_long`); desde un
shell de MinGW:

```bash
$ mkdir build && cd build
$ cmake -G "MSYS Makefiles" ..
$ make
$ cmake --install .           # desde un shell con privilegios, para Program Files
```

Otros ajustes útiles:

```bash
$ cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local ..   # instalar en otro lugar
$ cmake --install . --prefix /opt/v32opt         # ...o elegirlo al instalar
$ cmake --build . --target uninstall             # deshacer la última instalación
$ cmake -DDEST_LANG=fr ..                        # documentación en francés (fr) o español (es)
$ cpack                                          # .tar.gz (+ .deb/.rpm en Linux, .zip en Windows)
```

Las variables de caché `V32OPT_INSTALL_BINDIR`, `V32OPT_INSTALL_MANDIR` y
`V32OPT_INSTALL_DOCDIR` cambian cada destino por separado (relativos al
prefijo).

### Compilación directa

También puedes compilar el código modular directamente con GCC o Clang
(las cabeceras están en `inc/`, y las pasadas de mirilla en su propio
subdirectorio):

```bash
# Compila todos los fuentes con la optimización activada
$ gcc -O2 -Wall -Wextra -Iinc src/*.c src/peephole/*.c -o v32opt

# Comprueba la compilación (ejecutarlo sin argumentos muestra la
# información de uso)

$ ./v32opt
```

Hay una referencia completa de la línea de comandos en forma de página de
manual de unix en `man/v32opt.1` (puedes verla con `man ./man/v32opt.1`).

### Idiomas de la documentación

Este README y la página de manual están disponibles en inglés, español y
francés: `README.md` / `README.es.md` / `README.fr.md`, y `man/v32opt.1` /
`man/v32opt.es.1` / `man/v32opt.fr.1` (`man ./man/v32opt.es.1` para leer
una sin instalarla). Ambas compilaciones instalan por defecto la página de
manual en inglés; `DEST_LANG=es` o `DEST_LANG=fr` (`make sysinstall
DEST_LANG=es`, `cmake -DDEST_LANG=es ..`) instala esa traducción en su
lugar, con el nombre simple `v32opt.1`, de modo que `man v32opt` la
muestra directamente. Donde CMake instala también el README (Windows), el
traducido se instala como `README.md`.

### Binarios precompilados

El flujo de trabajo de GitHub Actions `Generate Global Binaries`
(`.github/workflows/release-builds.yml`, que se lanza a mano desde la
pestaña Actions) compila binarios comprimidos en zip para Linux (x86_64,
arm64, Raspberry Pi armv7/armv8), macOS (x86_64, arm64) y Windows (x86_64,
arm64), además de -- como extras opcionales que nunca retrasan una
publicación -- OpenBSD (x86_64, arm64) y Haiku (x86_64). Cada zip contiene
el binario, los README y las páginas de manual. Antes de compilar, el
flujo comprueba que las páginas de manual llevan la versión actual (véase
[Publicación de versiones](#publicación-de-versiones-sellado-de-versión));
las compilaciones nativas pasan una prueba rápida (`--version` y
`testing/long-lines/run.sh`).

Una ejecución de tipo `release` reúne los zip, junto con un
`SHA256SUMS.txt`, en una publicación (release) de GitHub `v<VERSIÓN>`,
cuyo texto es `.github/release-notes.md` (`@VERSION@` se sustituye por la
versión). La publicación es un **borrador** hasta que se pulsa «Publish
release» en la página Releases; marque `publish_release` al lanzar la
ejecución para publicarla de inmediato. Una versión `-dev` se marca como
preliminar (pre-release). Repetir la ejecución para la misma versión
actualiza esa publicación.

---

## Integración en el proceso de construcción del CART

La secuencia habitual para construir un CART de Vircon32 consta de los
siguientes pasos (si escribes en ensamblador puedes empezar en el paso 2):

### escribir/editar el código fuente

Actualmente Vircon32 ofrece un compilador de C (considerado estable y el
lenguaje principal de desarrollo en la plataforma) a través de su
conjunto de herramientas `DevTools`. También existen (en desarrollo)
compiladores de terceros para Lua (`v32lua`) y C++ (`v32c++`, que traduce
C++ a C para el compilador de C de Vircon32).

### compilar el código fuente

Cada compilador traduce el código y la sintaxis de su lenguaje de alto
nivel a ensamblador de Vircon32. Es este ensamblador lo que necesitas
antes de poder continuar con la optimización mediante `v32opt`.

```bash
# compila un programa en C con el compilador de C de Vircon32
$ compile -o game.asm game.c

# compila un programa en lua con el compilador v32lua
$ v32lua -o game.asm game.lua

# compila un programa en C++: v32c++ genera C, que luego compila el compilador de C
$ v32c++ -o game.c game.cpp
$ compile -o game.asm game.c
```

Qué modo `-L` usar (y qué vigilar) depende de dónde proviene el
ensamblador -- véase [Lenguajes de
Origen](#lenguajes-de-origen-c-c-lua-y-ensamblador).

Si estás escribiendo un programa de Vircon32 DIRECTAMENTE en lenguaje
ensamblador, puedes pasar directamente al siguiente paso (optimizar).

Si quieres intentar optimizar un CART binario de Vircon32 ya empaquetado
(quizá no tengas acceso al código fuente para construirlo desde cero),
puedes usar las órdenes `unpackrom` y `disassemble` que proporcionan las
`DevTools` de Vircon32 para obtener el ensamblador desensamblado de
cualquier CART -- pero lee primero la advertencia sobre el desensamblado
en [Lenguajes de Origen](#lenguajes-de-origen-c-c-lua-y-ensamblador): la
mayoría de las optimizaciones rompen los programas desensamblados.

Comprueba que tienes el archivo `game.asm` resultante (y, si te interesa
anotar el ahorro de espacio que se puede conseguir con la optimización,
toma nota del tamaño total del archivo de ensamblador).

### optimizar: pasa tu archivo de ensamblador por v32opt

Con un archivo de ensamblador en la mano, pásalo por `v32opt` aplicando
la combinación de optimizaciones que desees.

Se recomienda probar varias ejecuciones, con distintas optimizaciones,
para encontrar el "punto óptimo" deseado (la mayor mejora de rendimiento o
ahorro de espacio sin que nada se rompa). A veces tendrás suerte y podrás
aplicar todas las optimizaciones y simplemente funcionará. Otras veces
tendrás que dar marcha atrás, aislar la optimización u optimizaciones
problemáticas y excluirlas.

Para empezar, prueba tu archivo de ensamblador con `-O1`, creando un nuevo
archivo de ensamblador `gameOpt.asm` (NO sobrescribas el original):

```bash
# pasa game.asm por v32opt con las optimizaciones de -O1:
$ v32opt game.asm -o gameOpt.asm -O1
```

NOTA: el archivo de entrada es el único argumento que no es una opción, y
puede aparecer antes, después o entre las opciones (`v32opt -O1 -o
gameOpt.asm game.asm` funciona igual, en todas las plataformas). Usa `-o`
para dar nombre a la salida; sin ella, `game.asm` se escribe en
`gameOpt.asm`. Un segundo nombre de archivo es un error, en lugar de
ignorarse en silencio.

También puedes ejecutar `v32opt` con el argumento `-v`, y te dará un
informe general de las acciones de optimización que ha podido realizar:

```bash
# pasa game.asm por v32opt con las optimizaciones de -O1, mostrando estadísticas:
$ v32opt game.asm -o gameOpt.asm -O1 -v
```

Ahora deberías tener `gameOpt.asm` (o como hayas decidido llamarlo). Esta
es la forma "optimizada" de `game.asm`, resultado del trabajo de `v32opt`
sobre él.

Compara los tamaños de los archivos: el ahorro de espacio es una ganancia
de optimización habitual con relativamente poco esfuerzo (en la mayoría
de los casos, el archivo de ensamblador optimizado debería ser algo más
pequeño).

### continuar con el ensamblado

Llegado este punto, puedes continuar con el proceso de construcción de tu
CART de Vircon32, pasando el código de ensamblador optimizado (en
`gameOpt.asm`) por el ensamblador de Vircon32 para obtener el archivo
objeto resultante (`game.vbin` o `gameOpt.vbin`):

```bash
# ensambla gameOpt.asm
$ assemble -o gameOpt.vbin gameOpt.asm
```

Si todo va bien, el archivo de ensamblador optimizado debería ensamblarse
igual de bien que el `game.asm` original (se supone que sin problemas).

### generar los recursos del CART

Genera las texturas y los sonidos que vaya a incluir tu CART y crea tu
archivo de definición del CART `game.xml` (ten en cuenta que `v32lua`
genera automáticamente un archivo XML por defecto):

```bash
$ png2vircon -o background.vtex background.png
$ png2vircon -o sprites.vtex sprites.png
...
```

TEN EN CUENTA que tu archivo XML probablemente espera que el `.vbin` se
llame como el archivo de ensamblador original (`game.asm` -> `game.vbin`),
así que, o bien renombras el archivo `gameOpt.vbin`, o bien editas
`game.xml` para que use `gameOpt.vbin`. O, si te interesa comparar
rendimientos, haz una copia del XML, llámala `gameOpt.xml` y edítala para
que funcione con tu archivo optimizado. Así podrás construir ambos (sin
optimizar y optimizado) y comparar los resultados.

### empaquetar el CART

Por último, una vez que tengas todas las piezas en su sitio (el archivo
`.vbin`, los archivos `.vtex`/`.vsnd` que haya y el archivo `.xml`), puedes
construir el CART:

```bash
# construye el CART
$ packrom game.xml
```

En este punto, si no ha habido errores, deberías tener un `game.v32` que
puedes jugar en el emulador de Vircon32.

---

## Lenguajes de Origen: C, C++, Lua y Ensamblador

`v32opt` solo ve ensamblador, pero importa cómo se produjo ese
ensamblador. Las pasadas reconocen las formas de código y las
convenciones de etiquetas que generan los compiladores (etiquetas de
función `__function_<nombre>:`, sus etiquetas internas `..._return:`, el
marco estándar `PUSH BP` / `MOV BP, SP`, las directivas `pointer` para
direcciones de funciones, las cabeceras de bucle `..._start:`), y el modo
`-L` indica a las pasadas que siguen valores cómo interpretar los
inmediatos.

| Origen | Cadena de herramientas | Modo `-L` | Notas |
| --- | --- | --- | --- |
| C | `compile` (DevTools de Vircon32) | `c` (por defecto) | El objetivo principal; todas las pasadas están diseñadas en torno a esta salida. |
| C++ | `v32c++` → C → `compile` | `c` (por defecto) | El ensamblador *es* salida del compilador de C, así que todo lo dicho para C se aplica sin cambios (las vtables se convierten en tablas `pointer`, que DCE ya sigue). |
| Lua | `v32lua` | `lua` | Permite a las pasadas reconocer los valores NaN-boxed de v32lua (véase abajo). Procesar la salida de v32lua en modo C puede plegar, reenviar o eliminar por CSE la aritmética de empaquetado/desempaquetado (boxing/unboxing). |
| Ensamblador escrito a mano | — | `c` (por defecto) | Funciona, con las salvedades indicadas abajo. |
| CART desensamblado | `unpackrom` + `disassemble` | `c` | Solo es seguro sin ninguna pasada que cambie el tamaño -- véase abajo. |

### Modo Lua (`-L lua`)

`-L lua` (forma larga `--langmode lua`; el nombre del modo no distingue
mayúsculas de minúsculas) hace que las pasadas que siguen valores tengan
en cuenta el sistema de tipos empaquetados (boxed) de `v32lua`: las
construcciones de etiquetado/desetiquetado `BOXED_*` (OR/AND/IADD con un
inmediato empaquetado) nunca se pliegan, reenvían ni eliminan por CSE
como aritmética ordinaria; las etiquetas de NaN-boxing (`BOXED_*`,
`NAN_VALUE`, cualquier valor con el patrón de bits de NaN) nunca se
resuelven a través de `%define`; y la pasada de álgebra elimina la mitad
"¿es Nil?", demostrablemente muerta, de la prueba fija de veracidad del
compilador tras un productor de booleanos empaquetados. `-L c` (el valor
por defecto) mantiene el comportamiento simple del modo C. Las etiquetas
auxiliares internas `__global_*` de v32lua dentro del inicializador del
ámbito global se entienden en cualquiera de los dos modos.

### Ensamblador escrito a mano

Usa el modo C por defecto. Conviene saber:

* **Las pasadas a nivel de función se guían por los nombres del
  compilador.** DCE, la inserción en línea, la eliminación del puntero de
  marco y las pasadas promote-* solo tratan como función una etiqueta de
  la forma `__function_<nombre>:`. El código bajo otras etiquetas nunca se
  considera para eliminarlo ni insertarlo en línea -- es seguro, pero esas
  pasadas tendrán poco que hacer a menos que sigas la misma convención.

* **Todo lo que solo se alcanza mediante una dirección calculada es
  invisible.** Una rutina `__function_` cuya dirección solo se forma
  aritméticamente, o a la que se salta mediante `JMP R0`/`CALL R0` sin
  ninguna referencia `pointer`/de operando a su nombre, puede ser
  eliminada por DCE; el código tras un `JMP` incondicional al que solo se
  entra mediante un salto calculado (sin etiqueta) lo elimina
  `peephole-jumps`. Hacer referencia a la etiqueta por su nombre en
  cualquier lugar (`MOV R0, __function_cb`, una directiva `pointer`) la
  mantiene viva.

* **Las etiquetas que terminan en `_start`** las toma como cabeceras de
  bucle la pasada `promote-loops`. La seguridad del bucle se juzga sobre
  toda la `__function_` en la que está (el código anterior a la primera de
  esas etiquetas cuenta como una sola función): un solo `MOV R1, BP` o
  `LEA R1, [BP-N]` en ella mantiene en memoria todos los bucles de esa
  función.

* Las directivas del ensamblador (`%define`, `%include`, directivas de
  datos, ...) nunca las borra ninguna pasada, y los valores enteros de
  `%define` se ven a través del símbolo salvo que se indique
  `-fno-resolve-defines`.

### CARTs desensamblados

La herramienta `disassemble` de Vircon32 da nombre a los destinos de
saltos y llamadas (`_label1`, `_label2`, ...), pero toda referencia a
*datos* en la ROM del programa -- literales de cadena, tablas, valores
iniciales -- sigue siendo una dirección fija (`MOV R0, 0x2000002C`).
`v32opt` no reubica esas direcciones, así que **cualquier optimización que
cambie el tamaño del código desplaza los datos a los que apuntan** y el
CART reconstruido lee la memoria equivocada. (Eliminar un único
prólogo/epílogo de marco de 4 palabras basta para romper todas las
cadenas del programa.) Trata la optimización de código desensamblado como
un experimento: compara con cuidado el CART original y el optimizado, y
espera que solo sobrevivan los programas sin referencias a datos
residentes en la ROM. Como las funciones desensambladas se llaman
`_labelN` y no `__function_<nombre>`, DCE y la inserción en línea también
tienen poco que hacer ahí.

---

## Uso y Niveles de Optimización

```bash
v32opt <entrada.asm> [-o salida.asm] [opciones]
```

| Opción | Descripción | Pasadas Incluidas |
| --- | --- | --- |
| `-O0` | **Sin Optimización** | Desactiva todas las pasadas de optimización (por defecto). |
| `-O1` | **Mirilla Local** | Activa las 16 optimizaciones locales (14 de mirilla sobre ventana, más la **escritura muerta de registro** y la **limpieza de copias**, guiadas por vida de registros). |
| `-O2` | **Análisis Global** | Activa todas las pasadas de `-O1` + **CSE**, **Eliminación de Código Muerto (DCE)**, **Plegado Global de Constantes**, **Eliminación del Puntero de Marco** y **Promoción de Memoria a Registros** (`promote-regs`, `promote-leaf`, `promote-loops`). |
| `-O3` | **Agresivo** | Activa todas las pasadas de `-O2` + **Inserción de Funciones en Línea** + **Inversión de Bucles** + **Desenrollado de Bucles** (completo y parcial) + **Plegado del Índice**. |
| `-Os` | **Ahorro de Espacio** | `-O3` sin inversión de bucles (que duplica la prueba de cada bucle); marcador de posición para un nivel dedicado al tamaño. |
| `-o <archivo>` | **Archivo de Salida** | Dónde escribir el ensamblador optimizado (por defecto: `<entrada>Opt.asm`). |
| `-v`, `--verbose` | **Modo Detallado** | Muestra estadísticas detalladas de las pasadas y el número de optimizaciones de cada iteración. |
| `-t` | **Modo de Pruebas** | Imprime un resumen `pasada:recuento` legible por máquina (más una línea `total:N`). |
| `-d` | **Marcado de Depuración** | Marca cada optimización en la propia salida con comentarios `; [DEBUG <pasada>] ...`. |
| `-L <c\|lua>`, `--langmode` | **Modo de Lenguaje** | `lua` activa el reconocimiento de tipos NaN-boxed para la salida de `v32lua`; `c` (por defecto) para C, C++ y ensamblador (véase [Lenguajes de Origen](#lenguajes-de-origen-c-c-lua-y-ensamblador)). |
| `--dot <archivo>` | **Exportar el CFG** | Exporta el Grafo de Flujo de Control a un archivo `.dot` de Graphviz para visualizarlo. |
| `--trigger-max=<N>` | **Límite de Bisección** | Presupuesto global del total de transformaciones aplicadas (véase abajo). |
| `-V`, `--version` / `-h`, `--help` | **Información** | Imprime la versión, o un resumen de uso, y termina. |

`v32opt` no muestra nada si todo va bien, salvo que se indique `-v` o
`-t`, y termina con código de salida `1` (y un mensaje en stderr) ante una
opción incorrecta, una entrada que falta o no se puede leer, o una salida
que no se puede escribir. (Las líneas de entrada pueden tener
cualquier longitud.)

### Filosofía de los niveles de optimización y consideraciones de depuración

En `v32opt`, los niveles de optimización se distinguen no solo por cuánto
reducen el binario, sino por su complejidad de cálculo, su impacto
estructural y su facilidad de depuración:

* **`-O1` (Mirillas Locales):** Opera sobre pequeñas ventanas deslizantes
de instrucciones (algunas pasadas miran más adelante, con un límite de
distancia de búsqueda). Son pasadas sin estado y de tiempo lineal que
limpian artefactos evidentes del compilador sin impacto estructural en el
programa ni efecto en la facilidad de depuración — el nivel de menor
riesgo. Dicho esto, "menor riesgo" no significa "sin riesgo" (véase
`ISSUES` para las limitaciones conocidas que quedan), así que sigue
mereciendo la pena probar jugando la versión optimizada frente a la
original.

* **`-O2` (Análisis Global y Limpieza Estructural):** Introduce Grafos de
Flujo de Control (CFG), seguimiento del flujo de datos en todo el programa
y modificaciones del marco de pila. Estas pasadas analizan bloques y
funciones completos para eliminar código inalcanzable, plegar constantes a
través de saltos, suprimir sobrecarga redundante y mantener las variables
locales de pila en registros (las pasadas `promote-*` necesitan conocer la
función entera -- qué direcciones del marco escapan, qué registros están
demostrablemente libres --, por eso están aquí y no entre las pasadas de
mirilla).

* **`-O3` (Transformaciones Interprocedimentales Agresivas):** Realiza
modificaciones arquitectónicas profundas —como la inserción de funciones
en línea— que cambian tamaño del binario por velocidad de ejecución y
borran por completo los límites entre funciones.

#### Por qué la Eliminación del Puntero de Marco está en `-O2`

Desde un punto de vista puramente de diseño de compiladores, la
Eliminación del Puntero de Marco (`omit_frame_pointers`) parece una
optimización de `-O1` de manual: solo requiere un recorrido lineal rápido,
*siempre* reduce el número de instrucciones y el tamaño del binario, y
ofrece mejoras inmediatas de velocidad en las funciones hoja.

Sin embargo, la subimos deliberadamente a `-O2`. Los prólogos de función
estándar (`PUSH BP` seguido de `MOV BP, SP`) crean una lista enlazada
estable de marcos de pila en memoria. Cuando se eliminan los punteros de
marco, los emuladores y depuradores ya no pueden recorrer hacia atrás
desde `BP` para generar trazas limpias de la pila de llamadas durante los
fallos en tiempo de ejecución. Al situar `omit_frame_pointers` en `-O2`,
`-O1` sigue siendo un nivel de alto rendimiento y, a la vez, 100% "seguro
para depurar".

### Control individual de las optimizaciones

Puedes activar o desactivar pasadas concretas con `-f<nombre>` y
`-fno-<nombre>`:

```bash
# Ejemplo: ejecutar O2 pero desactivar las cadenas de saltos y añadir la promoción de registros en bucles
$ v32opt game.asm -O2 -fno-peephole-jmp-chain -fno-promote-loops
```

---

## Fase 1: Optimizaciones Locales de Mirilla (`-O1`)

La fase 1 opera sobre una ventana deslizante de instrucciones, limpiando
la salida redundante del compilador y simplificando secuencias locales de
instrucciones. Las optimizaciones de esta categoría se consideran las que
menos probabilidades tienen de romper gravemente el código resultante, y
aun así ofrecen cierta mejora de espacio o de rendimiento.

Si estás empezando con la optimización, prueba `-O1` y continúa con tu
construcción, comparando tanto el tamaño del archivo ASM resultante como
el comportamiento en ejecución. Si deseas más optimización (lo que puede
conllevar el riesgo de un ensamblador roto o un comportamiento erróneo en
ejecución), puedes avanzar a los niveles de optimización superiores.

### Eliminación de Pares de Instrucciones Adyacentes (`peephole-pairs`)

Examina pares consecutivos para eliminar operaciones redundantes, como
las instrucciones de Conversión de Entero a Booleano (`CIB`)
inmediatamente después de una comparación destructiva (`IEQ`/`INE`, que
ya dejan un `0` o `1` limpio en el registro de destino), las dobles
negaciones a nivel de bits (`BNOT`) y las operaciones de pila de efecto
neto nulo (`PUSH`/`POP`).

```vircon32
; ANTES                  ; DESPUÉS
IEQ R1, R2               IEQ R1, R2
CIB R1                   ; (CIB eliminado: IEQ ya produce 0 o 1)

BNOT R3
BNOT R3                  ; (La doble negación se anula)

PUSH R4
POP R4                   ; (Par PUSH/POP eliminado)
```

Quienes entiendan ensamblador verán que, en general, son secuencias
inútiles que consumen ciclos de CPU sin aportar nada a tu juego.
Eliminarlas significa menos ciclos consumidos por fotograma (y eso
*podría* mejorar el rendimiento).

### Simplificación Algebraica (`peephole-algebra`)

Elimina los movimientos de un registro a sí mismo (`MOV r, r`) y la
aritmética identidad (`IADD`/`ISUB` con `0`). También convierte las
multiplicaciones por 2 en sumas consigo mismo (`IADD r, r`) por claridad
idiomática.

```vircon32
; ANTES                              ; DESPUÉS
MOV R1, R1                           ; (Movimiento a sí mismo eliminado)

IADD R2, 0                           ; (Suma identidad eliminada)

IMUL R3, 2                           IADD R3, R3
```

Al igual que con la eliminación de pares, busca operaciones matemáticas
evidentes que no producen ningún cambio. Pueden quitarse con seguridad,
lo que reduce los ciclos por fotograma.

### Reenvío de Almacenamiento a Carga (`peephole-forwarding`)

Cuando un valor se almacena desde un registro en memoria y se vuelve a
cargar inmediatamente desde esa misma dirección de memoria en otro
registro, la lectura de memoria se sustituye por un movimiento directo de
registro a registro.

```vircon32
; ANTES                              ; DESPUÉS
MOV [R1+4], R2                       MOV [R1+4], R2
MOV R3, [R1+4]                       MOV R3, R2
```

Aunque esto quizá no aporte una mejora de rendimiento notable, debería
ahorrarte 1 palabra de espacio, ya que el `MOV` resultante entre dos
registros solo necesita 1 palabra para almacenar la instrucción, mientras
que cualquier referencia indirecta requiere una segunda palabra adicional
para el valor inmediato o la dirección.

### Eliminación de Recargas Redundantes (`peephole-compiler-myopia`)

Elimina la recarga de un valor que se acaba de almacenar desde el mismo
registro en la misma posición de memoria, cuando nada entre medias ha
cambiado la memoria ni los registros implicados.

```vircon32
; ANTES                              ; DESPUÉS
MOV [R1+4], R2                       MOV [R1+4], R2
MOV R2, [R1+4]                       ; (Recarga eliminada: R2 aún lo contiene)
```

Un artefacto habitual del compilador: el generador de código escribe un
temporal de vuelta en su posición de pila y lo vuelve a leer de inmediato,
antes de que nada haya podido cambiarlo.

### Eliminación de Saltos Redundantes (`peephole-jumps`)

Tres limpiezas locales del flujo de control: elimina los
`JMP`/`JT`/`JF` que apuntan directamente a la etiqueta que sigue
inmediatamente a la instrucción; invierte los pares de rama sobre salto
(`JF R, L1; JMP L2; L1:` se convierte en `JT R, L2; L1:`); y elimina el
código que un `JMP` incondicional vuelve inalcanzable (hasta la siguiente
etiqueta, que se conserva).

```vircon32
; ANTES                              ; DESPUÉS
JMP loop_continue                    ; (Salto a la instrucción siguiente eliminado)
loop_continue:                       loop_continue:
```

### Eliminación de Movimientos Redundantes y Simétricos (`peephole-movs`)

Elimina los movimientos duplicados consecutivos (`MOV R1, X; MOV R1, X`)
y los movimientos "espejo", en los que dos registros intercambian valores
dos veces sin modificación (`MOV R1, R2; MOV R2, R1`).

```vircon32
; ANTES                              ; DESPUÉS
MOV R1, 100                          MOV R1, 100
MOV R1, 100                          ; (Movimiento duplicado eliminado)

MOV R2, R3                           MOV R2, R3
MOV R3, R2                           ; (Movimiento espejo eliminado)
```

### Combinación de Operaciones con Inmediatos (`peephole-immediates`)

Combina sumas o restas sucesivas sobre el mismo registro con operandos
inmediatos en una sola operación combinada. Si las operaciones se anulan
a cero, se eliminan ambas.

```vircon32
; ANTES                              ; DESPUÉS
IADD R1, 5                           IADD R1, 2
ISUB R1, 3
IADD R2, 10                          ; (Ambas eliminadas: +10 -10 = 0)
ISUB R2, 10
```

### Reducción de Fuerza (`peephole-reduce`)

Simplifica operaciones aritméticas con constantes especiales. Las
multiplicaciones por `0` se convierten en `MOV 0`, las multiplicaciones o
divisiones por `1` se eliminan, y las multiplicaciones por `2` se
convierten en sumas. En las formas de coma flotante solo se aplican las
identidades exactas (`FMUL r, 1.0`, `FDIV r, 1.0`) — `FMUL r, 0.0` se deja
deliberadamente como está, ya que `Inf/NaN * 0.0` es `NaN`, no `0.0`.

```vircon32
; ANTES                              ; DESPUÉS
IMUL R1, 0                           MOV R1, 0
IMUL R2, 1                           ; (Multiplicación identidad eliminada)
IDIV R3, 1                           ; (División identidad eliminada)
IMUL R4, 2                           IADD R4, R4
```

En muchas otras CPU, las instrucciones de multiplicación y división
pueden ser más "costosas" en ciclos. En Vircon32 eso no es un problema.
Aun así, simplificar las operaciones puede ayudar a la legibilidad
general.

### Optimizaciones de Desplazamiento (`peephole-shifts`)

Elimina los desplazamientos nulos por `0` y convierte los desplazamientos
a la izquierda de `1` (`SHL r, 1`) en sumas consigo mismo (`IADD r, r`).

```vircon32
; ANTES                              ; DESPUÉS
SHL R1, 0                            ; (Desplazamiento por 0 eliminado)

SHL R2, 1                            IADD R2, R2
```

### Plegado de Comparaciones con Cero (`peephole-zero-test`)

Los saltos condicionales de Vircon32 ya comparan un registro con cero
(`JT` salta cuando no es cero, `JF` cuando es cero), así que calcular
antes la comparación es redundante. Esta pasada integra la comparación en
el salto, tanto cuando se comprueba una copia como cuando el valor se
comprueba en su sitio.

```vircon32
; ANTES                              ; DESPUÉS
MOV R0, R4
IEQ R0, 0
JT  R0, __slow_path                  JF  R4, __slow_path

INE R5, 0
JF  R5, __is_zero                    JF  R5, __is_zero
```

El código original deja el resultado de la comparación (0 o 1) en el
registro auxiliar, así que la reescritura solo se aplica cuando está
demostrado que ese registro no se vuelve a leer en **ninguno** de los dos
lados de la rama. La demostración sigue los saltos y examina las
funciones llamadas; todo lo que no puede seguir (un salto o llamada
calculados, los registros `R11`-`R15`, una función llamada que pueda leer
el registro) deja el código intacto. Solo comparaciones de enteros: `FEQ`
trata `-0.0` como cero, mientras que `JF` comprueba los bits en bruto.

v32lua genera esta forma en cada búsqueda en tabla en línea, así que en
los programas en modo Lua elimina aproximadamente el 3% de todas las
instrucciones.

### Escrituras Muertas de Registro (`peephole-dead-regs`)

La salida del compilador (y de las pasadas promote) está llena de
instrucciones que solo escriben un registro que nadie vuelve a leer. Esta
pasada las elimina.

```vircon32
; ANTES                              ; DESPUÉS
MOV  R1, R6                          ; (R1 no se lee más)
ILT  R0, R6                          ILT  R0, R6
MOV  R0, R4                          ; (R0 se reescribe al inicio del bucle)
JMP  __loop                          JMP  __loop
```

Solo son candidatas las instrucciones cuyo único efecto es la escritura
(`MOV`, `LEA`, aritmética, desplazamientos, lógica, comparaciones,
conversiones): nunca `IN`/`POP`, divisiones, `MOD`, funciones
trascendentes, `MOVS`/`SETS`/`CMPS` ni `CALL`. Se elimina solo si
`liveness.c` (la consulta con flujo de control que usa `peephole-zero-test`)
demuestra que ningún camino lee el registro. `R0` está vivo en cada `RET`,
y también cualquier registro que un llamador directo lea tras la llamada
(el auxiliar de división sin signo de v32cc devuelve el resto en `R1`).

### Limpieza de Copias (`peephole-copies`)

Cancela las copias de registro que quedan tras mover un valor a un
registro para operar. Dos reescrituras sobre código lineal (sin etiqueta,
salto ni llamada):

```vircon32
; ANTES                              ; DESPUÉS
MOV  R1, R4                          IADD R4, 1
IADD R1, 1
MOV  R4, R1                          ; (copia de entrada, operación, copia de salida)

MOV  R1, R6                          ; (R1 ya muerto: eliminado)
ILT  R0, R1                          ILT  R0, R6
```

*Coalescencia:* `MOV Rt, Rs / ... / MOV Rs, Rt` alrededor de una ventana
que no menciona `Rs`, con `Rt` muerto después, renombra la ventana a `Rs`.
*Propagación:* tras `MOV Rt, Rs`, las lecturas de `Rt` hasta la siguiente
escritura de cualquiera de los dos leen `Rs`; la copia se va si `Rt` queda
muerto. Junto con `peephole-dead-regs` esto hace rentable la promoción.

### Eliminación de Almacenamientos Muertos (`peephole-dead-stores`)

Elimina los almacenamientos en memoria que se sobrescriben inmediatamente
con un almacenamiento posterior en la misma dirección de memoria
indirecta, sin una lectura intermedia.

```vircon32
; ANTES                              ; DESPUÉS
MOV [R1+0], R2                       ; (Almacenamiento sobrescrito eliminado)
MOV [R1+0], R3                       MOV [R1+0], R3
```

### Eliminación de Cargas Redundantes (`peephole-loads`)

Cuando dos registros cargan sucesivamente desde la misma dirección de
memoria indirecta, la segunda carga se sustituye por un movimiento
directo desde el registro de destino de la primera.

```vircon32
; ANTES                              ; DESPUÉS
MOV R1, [R2+8]                       MOV R1, [R2+8]
MOV R3, [R2+8]                       MOV R3, R1
```

De nuevo, el `MOV R3, R1` acabará ahorrando una palabra, ya que no
necesita hacer referencia a ningún dato inmediato.

### Plegado de Inmediatos (`peephole-immediate-prop`)

A pesar de su nombre, esta pasada pliega constantes *dentro* de un
registro, en lugar de propagarlas a otras instrucciones. Hace tres cosas
con inmediatos enteros sobre un registro de destino:

* elimina la aritmética identidad (`IADD`/`ISUB` por `0`, `IMUL`/`IDIV`
  por `1`);

* pliega un `MOV` de constante en un `IADD`/`ISUB`/`IMUL` posterior del
  mismo registro (nunca a través de una lectura o escritura de ese
  registro, de una rama condicional ni de un límite de
  etiqueta/salto/llamada);

* fusiona inmediatos `IADD`/`ISUB` adyacentes sobre el mismo registro,
  eliminando el par por completo cuando se anulan.

```vircon32
; ANTES                              ; DESPUÉS
MOV R1, 10                           MOV R1, 15
IADD R1, 5                           ; (Plegado en el MOV)

IADD R2, 5                           IADD R2, 2
ISUB R2, 3                           ; (Fusionado)
```

Un plegado cuyo resultado no cabría en un literal de 32 bits (la
aritmética se desbordó) se deja como está. `MOV R1, 42` / `IADD R2, R1`
*no* se reescribe como `IADD R2, 42` -- eso costaría una palabra en lugar
de ahorrarla.

### Eliminación de Cadenas de Saltos (`peephole-jmp-chain`)

Acorta la indirección de saltos. Todo `JMP`, `JT` o `JF` -- esté donde
esté en el programa -- cuya etiqueta de destino lleve directamente a otro
`JMP <etiqueta>` incondicional se redirige al final de la cadena, tenga
los saltos que tenga. Una vez redirigido el último usuario de la etiqueta
de un salto intermedio, ese salto intermedio se elimina.

```vircon32
; ANTES                              ; DESPUÉS
JT  R0, label_step1                  JT  R0, label_final
...                                  ...
JMP label_step1                      JMP label_final
...                                  ...
label_step1:                         label_step1:
JMP label_step2                      ; (Salto intermedio eliminado)
label_step2:                         label_step2:
JMP label_final                      ; (Salto intermedio eliminado)
```

"Lleva directamente a" admite otras etiquetas, comentarios y líneas en
blanco entre la etiqueta y su `JMP`; cualquier directiva termina la
búsqueda. Una cadena que vuelve sobre sí misma (`L: JMP L`) se deja como
está, y una que termina en un salto calculado (`JMP R0`) solo se sigue
hasta su última etiqueta.

NOTA: un salto intermedio solo se elimina cuando nada más puede
alcanzarlo: ninguna etiqueta situada justo encima se referencia en ningún
lugar del programa (como destino de salto o llamada, mediante
`MOV R0, etiqueta` que toma su dirección, mediante una directiva
`pointer`, ...), y ningún código puede caer en él (la instrucción anterior
debe ser a su vez una transferencia incondicional; un salto al principio
mismo del programa es el punto de entrada del arranque y siempre se
conserva). En caso contrario se conserva y sigue encaminando
correctamente a sus usuarios restantes.

---

## Fase 2: Flujo de Datos Global y Código Muerto (`-O2`)

La fase 2 construye un **Grafo de Flujo de Control (CFG)** sobre los
bloques básicos para realizar un análisis de flujo de datos en todo el
programa.

### Eliminación de Subexpresiones Comunes (CSE)

**Qué hace:** Detecta y elimina cálculos redundantes dentro de los
bloques básicos. Cuando se aplica la misma operación a los mismos
operandos de origen (tras inicializaciones `MOV` idénticas), la segunda
aparición se sustituye por un `MOV` desde el registro del primer
resultado. En Vircon32, donde todas las instrucciones tardan 1 ciclo,
esto **reduce el tamaño del código** al eliminar palabras duplicadas.

---

#### **Patrón**
```
MOV Rx, A    ; Calcula Rx = A
OP Rx, B     ; Calcula Rx = Rx OP B
...
MOV Ry, A    ; Reinicializa Ry = A (igual que Rx)
OP Ry, B     ; -- CSE sustituye esto por: MOV Ry, Rx
```

---

#### **Ejemplos**

| Antes | Después | Ahorro |
|--------|-------|---------|
| `MOV R1, R5`<br>`IADD R1, R2`<br>`MOV R3, R5`<br>`IADD R3, R2` | `MOV R1, R5`<br>`IADD R1, R2`<br>`MOV R3, R5`<br>`MOV R3, R1` | 0 palabras por sí solo (operación de 1 palabra → `MOV` de 1 palabra); véase abajo |
| `MOV R1, R5`<br>`IMUL R1, 42`<br>`MOV R3, R5`<br>`IMUL R3, 42` | `MOV R1, R5`<br>`IMUL R1, 42`<br>`MOV R3, R5`<br>`MOV R3, R1` | **1 palabra** (`IMUL` + inmediato = 2 palabras, `MOV` = 1) |
| `MOV R1, R5`<br>`FADD R1, R2`<br>`MOV R3, R5`<br>`FADD R3, R2` | `MOV R1, R5`<br>`FADD R1, R2`<br>`MOV R3, R5`<br>`MOV R3, R1` | 0 palabras por sí solo; véase abajo |

El `MOV R3, R5` de reinicialización queda muerto (se sobrescribe de
inmediato), y en `-O2` lo elimina `peephole-dead-stores` -- así que, en
una ejecución completa de `-O2`, cada fila anterior ahorra una palabra
más.

---

#### **Operaciones Soportadas**

Todas las operaciones aritméticas, lógicas y de coma flotante:

`IADD`, `ISUB`, `IMUL`, `IDIV`, `AND`, `OR`, `XOR`, `FADD`, `FSUB`,
`FMUL`, etc. Funciona con registros e inmediatos (incluidos los
negativos), y respeta:

- ✅ Los límites del flujo de control (`JMP`, `JT`, `JF`, `CALL`, `RET`, etiquetas)
- ✅ Las modificaciones de registros entre expresiones
- ✅ Las operaciones distintas (p. ej., `IADD` ≠ `ISUB`)

### Eliminación de Funciones Muertas (`dce`)

Realiza un análisis de alcanzabilidad a partir de las raíces conocidas
del programa -- el código anterior a la primera función (la ruta de
arranque), `__function_main` / `main` / `_start` / `start` / `__start`,
las rutinas de inicialización global (`__init_globals`,
`__function_init`, el inicializador del ámbito global de v32lua), las
etiquetas que contienen `ISR` o `interrupt`, las etiquetas de datos y toda
función nombrada en una directiva `pointer`. Cualquier función cuyo
nombre aparezca como operando de código alcanzable (un `CALL`, o un
`MOV R0, __function_cb` que toma su dirección) pasa a ser alcanzable a su
vez; las funciones (etiquetas de la forma `__function_<nombre>:`) que
nunca se alcanzan se eliminan.

```vircon32
; ANTES                              ; DESPUÉS
__function_main:                     __function_main:
    CALL __function_init                 CALL __function_init
    RET                                  RET

__function_unused:                   ; (Función inalcanzable eliminada)
    MOV R1, 0
    RET
```

Pensado sobre todo para el compilador de lua, ya que las versiones
recientes del compilador de C de Vircon32 ya realizan una forma de
eliminación de funciones muertas durante la compilación (C++ pasa por ese
mismo compilador de C).

### Propagación y Plegado Global de Constantes (`constant-folding`)

Usa un algoritmo de lista de trabajo basado en retículos sobre el CFG
para seguir las constantes conocidas de los registros a través de los
límites de los bloques. Pliega la aritmética constante y sustituye las
referencias a registros por valores inmediatos a través de saltos y
ramas, invalidando el estado de forma segura en las llamadas a funciones
(`CALL`) o en las escrituras indirectas.

```vircon32
; ANTES                              ; DESPUÉS
block_1:                             block_1:
    MOV R1, 10                           MOV R1, 10
    JMP block_2                          JMP block_2
block_2:                             block_2:
    MOV R2, R1                           MOV R2, 10
```

Ten en cuenta que esto cambia palabras por inmediatos (`MOV R2, 10` ocupa
dos palabras, mientras que `MOV R2, R1` ocupaba una), así que por sí solo
puede *aumentar* el binario ensamblado; su beneficio está en las
optimizaciones de mirilla basadas en inmediatos que habilita después.

### Eliminación del Puntero de Marco (`omit-frame-pointers`)

Examina cuerpos de función completos para comprobar si el registro
Puntero Base (`BP`) se referencia o desreferencia alguna vez en las
instrucciones de la función. En las funciones donde `BP` nunca se toca
(como pequeñas funciones matemáticas hoja, funciones de acceso o
funciones que solo usan los registros de propósito general `R0–R15`),
elimina por completo el prólogo estándar de la función (`PUSH BP`,
`MOV BP, SP`) y el epílogo (`MOV SP, BP`, `POP BP`). Cualquier referencia
relativa a BP cuenta como "tocar" BP — tanto las posiciones de parámetros
(`[BP+N]`) COMO las de variables locales (`[BP-N]`), ya que ambas acceden
a la memoria a través del puntero de marco.

Esto suprime 4 instrucciones redundantes de pila y memoria en cada
invocación, reduciendo notablemente la sobrecarga de ciclos y el tamaño
total del binario sin alterar la lógica de los registros.

```vircon32
; ANTES                  ; DESPUÉS
__function_add:          __function_add:
    PUSH BP                  ; (Preparación del marco en el prólogo eliminada)
    MOV BP, SP               ;
    IADD R1, R2              IADD R1, R2
    MOV R0, R1               MOV R0, R1
    MOV SP, BP               ; (Liberación del marco en el epílogo eliminada)
    POP BP                   ;
    RET                      RET
```

---

## Fase 3: Inserción en Línea Interprocedimental (`-O3`)

### Inserción de Funciones en Línea (`inline`)

Identifica funciones triviales (de ejecución corta y flujo de control
simple) y sustituye sus llamadas `CALL` directamente por el cuerpo de la
función. Esto elimina la sobrecarga de los saltos de llamada y retorno y
expone nuevas oportunidades de mirilla locales en el punto de llamada.

> **Opciones de diagnóstico:** Limita la inserción en línea con
> `-finline-max=N` o excluye funciones concretas con
> `-finline-exclude=<nombre>`.

```vircon32
; ANTES                              ; DESPUÉS
CALL __function_add_one              IADD R1, 1
...                                  ...
__function_add_one:                  __function_add_one:
    IADD R1, 1                           IADD R1, 1
    RET                                  RET
```

Esta optimización ha demostrado ser la de mayor impacto en cuanto a
mejoras apreciables (de rendimiento y de espacio, por cómo puede eliminar
un montón de instrucciones innecesarias relacionadas con la preparación y
la liberación de una función). Pero también se considera bastante
*agresiva* por el grado en que modifica tu código.


### Inversión de Bucles (`loop-invert`)

Ambos compiladores generan bucles con la prueba arriba, así que cada
iteración ejecuta dos saltos: el condicional de la prueba y el de retorno.
La inversión convierte el salto de retorno en una copia de la prueba más un
salto condicional directo al cuerpo, ahorrando un salto (un ciclo) por
iteración:

```vircon32
; ANTES                           ; DESPUÉS
__for_1_start:                    __for_1_start:
    MOV R0, R1                        MOV R0, R1
    ILT R0, R2                        ILT R0, R2
    JF  R0, __for_1_end               JF  R0, __for_1_end
    ...cuerpo...                  __v32opt_loop_1:
    JMP __for_1_start                 ...cuerpo...
__for_1_end:                          MOV R0, R1
                                      ILT R0, R2
                                      JT  R0, __v32opt_loop_1
                                  __for_1_end:
```

La copia superior de la prueba se conserva, así que la entrada al bucle y
cualquier otro salto al inicio (un `continue` en un `while`) siguen viendo
primero la prueba. Un bucle que se ejecuta cero o una vez no cuesta nada
extra; cada iteración adicional ahorra un ciclo, a cambio del tamaño de la
prueba una vez (como máximo 8 instrucciones).

Es simple duplicación de cola del código en el destino del salto, así que no
se lee ni escribe ningún registro que no se hiciera antes. Solo se reescribe
un salto de retorno si la prueba es código lineal (sin etiqueta, `CALL` ni
otro salto), el salto es hacia atrás, el código tras el salto empieza con la
etiqueta de salida de la prueba, las etiquetas están definidas una sola vez
(las etiquetas `.local` por defecto de v32cc se repiten por función: usa
`--standard`) y el destino no es la entrada de una función. Se ejecuta una
vez, tras las pasadas promote (que reconocen un bucle por su salto de
retorno) y antes de la limpieza final. Forma parte de `-O3`, no de `-O2` ni
`-Os`, porque cambia tamaño de código por velocidad; `-floop-invert` la
activa por separado.


### Desenrollado Completo de Bucles (`loop-unroll`)

Un bucle contado cuyo número de iteraciones es una constante de compilación
se sustituye por esa cantidad de copias de su cuerpo, sin prueba ni salto de
retorno (véase el ejemplo en la versión en inglés):

```vircon32
; ANTES                           ; DESPUÉS (4 iteraciones)
    MOV R4, 0
__for_1_start:                        ...cuerpo, R4 = 0...
    MOV R0, R4                        IADD R4, 1
    ILT R0, 4                         ...cuerpo, R4 = 1...
    JF  R0, __for_1_end               IADD R4, 1
    ...cuerpo...                      ...                (2 copias más)
    IADD R4, 1                    __for_1_end:
    JMP __for_1_start
__for_1_end:
```

Por iteración se eliminan la prueba (3 instrucciones) y el salto (1). Cada
copia conserva su `IADD Ri, paso`, así que el contador vale lo mismo que
antes en todas partes, y en cada copia las lecturas del contador donde se
acepta un inmediato se sustituyen por su valor. Condiciones: el número de
iteraciones se calcula con el valor inicial y el paso constantes; el cuerpo
no escribe el contador ni contiene `CALL`; la etiqueta de inicio solo se
referencia desde el salto de retorno y las etiquetas del cuerpo solo desde
dentro (las copias reciben etiquetas nuevas); el registro de la prueba se
recrea (`MOV Rc, 1` al inicio de cada copia, `MOV Rc, 0` en la salida normal)
cuando la vida de registros muestra que alguien lo lee. Máximo 16
iteraciones y 96 instrucciones; los bucles anidados se desenrollan de dentro
hacia fuera. Se ejecuta una vez, tras el plegado de constantes y antes de
`loop-invert`; forma parte de `-O3` (no de `-Os`, porque el código crece) y
`-floop-unroll` la activa por separado.


### Plegado del Índice en Bucles (`loop-index`)

El código con arreglos direcciona elementos como `base + i`. Cuando el
contador de un bucle contado solo se usa para formar direcciones de una misma
base, esa base se pliega en el propio contador, sin registro extra (véase el
ejemplo en la versión en inglés): se añade `IADD Ri, BASE` una vez antes del
bucle, la cota inmediata se desplaza por BASE en tiempo de compilación y cada
par `MOV Rt, BASE / IADD Rt, Ri` pasa a `MOV Rt, Ri`. Una instrucción menos
por uso e iteración. Condiciones: toda otra mención del contador es la segunda
mitad de un par adyacente de esa misma base (un número o un `%define` que lo
resuelva); la cota es inmediata; el cuerpo no tiene `CALL`; el contador está
muerto en la etiqueta final, en cada salto hacia fuera y en cada `RET`; y a la
etiqueta de inicio solo se llega por continuación. Se ejecuta una vez, tras
`loop-unroll` y antes de `loop-invert`; forma parte de `-O3`, y
`-floop-index` la activa por separado.

### Desenrollado Parcial de Bucles (`loop-unroll-partial`)

Un bucle contado cuyo número de iteraciones **no** se conoce en compilación
paga la prueba y el salto en cada iteración. Esta pasada ejecuta K iteraciones
(4, o 2 si el cuerpo es grande) por vuelta de un nuevo bucle principal, tras una
sola guarda que comprueba que las K pasarán la prueba, y conserva el bucle
original para terminar las 0..K-1 iteraciones restantes (véase el ejemplo en la
versión en inglés). De cada 4 iteraciones se eliminan 3 pruebas y 3 saltos. La
guarda usa solo el registro de la prueba original: una cota inmediata se
desplaza en compilación; una cota en registro o posición de marco usa
`MOV Rc, cota / ISUB Rc, (K-1)*paso / I<espejo> Rc, Ri`. Condiciones: la
comparación concuerda con el sentido del paso (`ILT`/`ILE` con paso positivo,
`IGT`/`IGE` con negativo; `INE` se deja); la cota no cambia dentro del cuerpo
(inmediata; registro que el cuerpo no escribe; posición `[BP+N]` que ninguna
instrucción del cuerpo escribe, en una función cuya dirección de marco nunca
escapa); el cuerpo no tiene `CALL`, no escribe el contador salvo en su paso y
tiene como máximo 24 instrucciones; los registros temporales de la prueba están
muertos a la entrada del cuerpo, en la etiqueta final y en cada salto hacia
fuera; las etiquetas del cuerpo se referencian solo desde dentro y cada copia
recibe nombres nuevos. Se ejecuta tras `loop-unroll` y `loop-index`, antes de
`loop-invert`; forma parte de `-O3`, y `-floop-unroll-partial` la activa por
separado.


### Ajuste de las pasadas de bucles

Los límites de las pasadas de bucles son opciones de línea de órdenes
(`-f<nombre>=N`), de modo que cada programa pueda llevarse a su punto óptimo
(`testing/bench/bench.py --tune` automatiza la búsqueda): `loop-unroll-max-trips`
(16, iteraciones que `loop-unroll` elimina por completo), `loop-unroll-max-size`
(96, instrucciones del bucle desenrollado), `loop-unroll-partial-factor` (0,
copias por vuelta, 2..8; 0 = 4, o 2 para cuerpos grandes),
`loop-unroll-partial-max-body` (24), `loop-unroll-partial-big-body` (12),
`loop-unroll-partial-min-body` (1) y `loop-invert-max-test` (8, prueba más larga
que `loop-invert` duplica). Los valores se limitan a rangos razonables.

---

## Promoción de Memoria a Registros (`-O2`)

El compilador de C de Vircon32 mantiene cada variable local en memoria
(`[BP-1]`, `[BP-2]`, ...) y pasa por un registro en cada uso. Tres pasadas
mueven los locales más usados a registros libres (`R1`-`R10`) durante el
tramo de código en que es seguro. Por sí sola cuesta los mismos ciclos;
lo que rinde es que las copias que deja las cancelan después las pasadas
de `-O1` `peephole-copies` y `peephole-dead-regs`. Las tres forman parte
de `-O2` (y por tanto de `-O3` y `-Os`); `-fno-promote-regs`,
`-fno-promote-leaf` y `-fno-promote-loops` dejan fuera la que se indique.

### Promoción de Posiciones de Pila (`promote-leaf` / `promote-regs`)

* `promote-leaf` trabaja sobre **funciones hoja** completas (sin `CALL`):
  la carga va justo tras el prólogo.
* `promote-regs` aplica lo mismo a funciones ordinarias, por separado en
  cada tramo sin `CALL`, y solo si el tramo tiene una entrada y una salida.
  Un local escrito en un tramo se guarda justo antes del `CALL` que lo
  termina, si algo lo lee después.

Antes de `RET` no se guarda nada: el marco está muerto y ningún puntero
puede apuntar a él (véase más abajo).

### Promoción a Registros Invariante en Bucles (`promote-loops`)

Actúa sobre bucles sin `CALL` y promueve los locales usados en el cuerpo:
la carga va antes de la etiqueta de cabecera (un preheader real) y el
almacenamiento tras la etiqueta de salida. Se omite la carga si todo
camino escribe el registro antes de leerlo, y el almacenamiento si nada
fuera del bucle lee la posición (salvo en un bucle anidado, que se
reentra). Las referencias se ponderan por el anidamiento (x8 por nivel),
así que con pocos registros ganan los bucles más internos. Un bucle que
no se puede promover se omite solo; los anidados se siguen considerando.

### Convención de llamada: ¿de quién son los registros?

El compilador estándar de Vircon32 es **callee-save**: una función guarda
(`PUSH`) cada registro que usa y lo restaura antes de `MOV SP, BP`, porque
sus llamadores mantienen temporales vivos en `R1`, `R2`, ... a través de
un `CALL`. Un local promovido a un registro que la función no guardó
destruiría el valor del llamador. Por tanto, fuera de `-L lua`:

* un registro que la función ya guarda se usa gratis;
* si no, se añade un `PUSH` tras los existentes y un `POP` antes de los
  existentes en **cada** salida (solo si todas las salidas son epílogos
  reconocibles y las referencias justifican las 2 instrucciones extra;
  `R4`-`R10`);
* si no es posible, el local se queda en memoria.

`-L lua` es **caller-save** (v32lua guarda alrededor de las llamadas), así
que no hace falta guardar nada. La salida de v32cc se trata como la del
compilador estándar: a lo sumo un par innecesario, nunca un resultado
erróneo.

### Qué mantiene una posición en memoria

Estas comprobaciones miran **toda la función**, no solo la región:

* La dirección del marco escapa: `MOV R1, BP`, `LEA R1, [BP-N]`,
  `IADD R1, BP`, ...; igual para cualquier copia de `SP`.
* No hay prólogo `MOV BP, SP`.
* Un tramo o bucle al que algo salta desde fuera, o desde dentro hacia
  un punto que se saltaría el almacenamiento.
* Los parámetros (`[BP+N]`) nunca se promueven.

### Elección del registro

Solo se usa un registro **demostrablemente libre**: no aparece en la
región y la consulta de vida (`liveness.c`) muestra que no se lee antes
de reescribirse en ningún camino desde la carga. `R11`-`R13` (operandos
implícitos de `MOVS`/`SETS`/`CMPS`) nunca se usan.

### Qué esperar

Sobre unos 190 programas generados por compilador (y ~175 más por el
compilador estándar), ciclos / tamaño frente a `-O0`:

| | v32cc | `compile` estándar |
| --- | --- | --- |
| `-O2` antes de promote y la limpieza | -1,6% / -3,1% | -1,6% / -5,6% |
| `-O1` (ahora con la limpieza) | -3,4% / -3,8% | -3,0% / -6,1% |
| `-O2` | -6,9% / -6,0% | -4,9% / -14,5% |
| `-O3` | -7,9% / -6,0% | -7,0% / -14,4% |

`-d` marca cada carga y almacenamiento añadido
(`; [DEBUG promote-regs] ...`), y `--trigger-max` cuenta una posición por
variable promovida, de modo que un fallo se puede biseccionar como en
cualquier otra pasada.

---

## Visualización del CFG

### Exportar Grafos de Flujo de Control

Puedes generar diagramas visuales del grafo de flujo de control de tu
ensamblador con el parámetro `--dot`:

```bash
v32opt game.asm -O2 --dot cfg.dot
dot -Tpng cfg.dot -o cfg.png
```

Esto exporta bloques, etiquetas, listas de instrucciones y aristas de
ramificación dirigidas (cubriendo las caídas a la instrucción siguiente,
los saltos incondicionales, las ramas condicionales (`JT`/`JF`) y las
llamadas) en formato estándar de Graphviz.

---

## Salvaguardas de Seguridad y Corrección

Para evitar que las optimizaciones alteren la semántica del programa o
rompan comportamientos en casos límite, el optimizador impone
comprobaciones estructurales de seguridad estrictas:

* **Preservación de los literales de coma flotante:** Los inmediatos de
coma flotante (p. ej., `0.500000`) se distinguen explícitamente de los
inmediatos enteros. Las pasadas de seguimiento de constantes tratan los
literales de coma flotante como desconocidos (`VAL_BOTTOM`) en lugar de
interpretarlos como `0` enteros. Esto evita que el plegado de constantes
corrompa en silencio argumentos de coma flotante (como los volúmenes de
los canales de audio o los cálculos de física).

* **Rango de los literales:** Todo plegado de constantes se calcula con
precisión ampliada y se abandona si el resultado no cabe en un literal de
32 bits que acepte el ensamblador de Vircon32, así que una cadena de
`IMUL`/`IADD` que se desborda se deja tal como está escrita en lugar de
producir un inmediato fuera de rango.

* **Las directivas nunca se borran:** Ninguna pasada elimina una
directiva del ensamblador (`%define`, `%include`, datos
`integer`/`float`/`string`/`pointer`/`datafile`), aunque esté dentro de un
tramo de código muerto que se esté eliminando. Una línea puede tener
cualquier longitud: el texto de cada línea se conserva entero (un literal
`string` largo, o una tabla `pointer` con cientos de funciones, es una sola
línea), y DCE lee una línea `pointer` completa, de modo que una función
nombrada solo al final de ella se conserva igualmente.

* **Protección de cargas autorreferenciales:** La eliminación de
movimientos redundantes examina a fondo las cargas indirectas de memoria.
Instrucciones textualmente idénticas como `MOV R1, [R1]` seguida de un
segundo `MOV R1, [R1]` se reconocen como un encadenamiento de
desreferencias de punteros y no como duplicados, lo que garantiza que las
búsquedas de puntero a puntero se evalúen correctamente.

* **Salvaguardas ante la toma de direcciones:** Las pasadas de promoción
a registros buscan en la función entera (no solo en el bucle o tramo que
se promueve) cálculos de direcciones de pila (p. ej., `IADD R1, BP`,
`LEA R1, [BP-N]`, `MOV R1, SP`). Si la dirección de memoria de una
variable local se calcula dinámicamente o se toma en cualquier punto de
ella, la promoción se cancela automáticamente para esa función, para
garantizar la seguridad de la memoria y evitar errores de solapamiento
(aliasing). La elección del registro se apoya en una demostración de vida
de registros (véase
[Promoción de Memoria a Registros](#promoción-de-memoria-a-registros--o2)).

* **Reescritura de la pila en la inserción en línea:** Cuando se insertan
en línea funciones hoja, las lecturas de parámetros que acceden a
`[BP+N]` (con `N >= 2`) se reescriben dinámicamente como `[SP+(N-2)]` en
el punto de llamada. Esto permite insertar los cuerpos de las funciones
llamadas sin corromper los marcos de pila de quien llama ni necesitar un
puntero de marco dedicado.

* **Estas salvaguardas no son exhaustivas.** Las pasadas de ventana
modelan ahora las instrucciones de lectura-modificación-escritura en
memoria (`IADD [R1+0], 5`), los almacenamientos solapados a través de
registros base distintos y las operaciones de memoria dinámica
`MOVS`/`SETS`/`CMPS`, y `CALL`/`RET` se tratan como movimientos de SP (el
apilado y desapilado de la dirección de retorno por hardware). Las
limitaciones conocidas que quedan se registran en el archivo `ISSUES` (en
particular: la eliminación de código inalcanzable no intenta razonar
sobre destinos de salto calculados — `JMP R0` / `JMP 0x1000` — y la
heurística del sufijo `_return:` de DCE puede confundir una función real
cuyo nombre termine casualmente en `_return`). Verifica siempre la salida
optimizada construyéndola y ejecutándola junto a la original.

---

## Opciones de Diagnóstico y Depuración

Al trabajar con bases de código complejas o al aislar problemas en tiempo
de ejecución causados por transformaciones agresivas, usa las siguientes
opciones de diagnóstico para controlar el comportamiento del optimizador:

* **`-finline-max=N`**

Limita el tamaño de una función que todavía se considera insertable en
línea, en instrucciones del cuerpo: cualquier función cuyo cuerpo lineal
tenga más de `N` instrucciones nunca se inserta en línea. El valor se
limita al intervalo `0..32` (la capacidad de la tabla de candidatas).

*(Por defecto: `8`).*

* **`-finline-call-limit=N`**

Limita a `N` el número total de llamadas `CALL` insertadas en línea en
toda la ejecución (evaluadas en el orden del archivo). Ajustando este
número puedes hacer una bisección de las llamadas insertadas para aislar
errores que solo aparecen en ejecución.

*(Por defecto: `-1`, es decir, sin límite).*

* **`-finline-exclude=NOMBRE`**

Excluye funciones concretas de la inserción en línea. Admite una lista
de nombres de etiquetas separados por comas.

*(Ejemplo: `-finline-exclude=__function_play_audio,__function_update_physics`).*

* **`-fmax-passes=N`**

Limita el motor iterativo de optimización local a un máximo de `N`
pasadas (por defecto: `1000`).

* **`--trigger-max=N`**

Un presupuesto global de transformaciones compartido por TODAS las
pasadas activadas y por cada iteración de punto fijo: una vez aplicadas
`N` transformaciones en total (nodos eliminados, reescrituras en el
sitio, cargas/almacenamientos insertados), todo candidato posterior se
deja intacto, como si nunca hubiera coincidido. Omítela (o pasa un `N`
negativo) para no tener límite. Es la herramienta principal para
localizar por bisección una compilación errónea: vuelve a ejecutar con
valores crecientes de `N` hasta que la salida se rompa — la `N`-ésima
transformación aplicada, en el orden del programa a través de todas las
pasadas, es la que hay que examinar. Combínala con `-d` para que el
culpable aparezca nombrado en el propio archivo de salida, y con `-v` para
obtener un informe final de cuántas transformaciones se usaron.

---

## Publicación de versiones (sellado de versión)

La versión está en un único lugar: el `#define` `VERSION` de
`inc/v32opt.h` (el mismo esquema `AAAAMMDD-estado` que `v32lua` y
`v32c++`, p. ej. `20261002-dev`, `20261015-release`). `v32opt --version`
la imprime, la compilación con CMake la lee al configurar (para los
nombres de los paquetes), y `make version` la sella en las páginas de
manual:

```bash
# tras editar el #define a mano:
$ make version

# o deja que make edite también inc/v32opt.h:
$ make version VERSION=20261015-release

# después recompila, para que el binario muestre la nueva versión
$ make
```

`make version` imprime la versión y las líneas que mantiene: el
`#define` de la cabecera y la línea `.TH` de cada página de manual
(inglés, español y francés), que también recibe el mes y el año actuales,
en el idioma de esa página.
