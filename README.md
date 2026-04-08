# Proyecto #1 — Arreglos Paginados

Este proyecto implementa un generador de archivos binarios con números enteros aleatorios y un programa de ordenamiento que trabaja sobre un arreglo paginado (`PagedArray`).  
La idea principal es simular el comportamiento de un arreglo grande que no se mantiene completo en memoria principal, sino que se administra por páginas, cargando y descargando información según sea necesario.

---

## Objetivo

Desarrollar una solución en C++ que permita:

- generar archivos binarios con enteros aleatorios,
- ordenar archivos binarios grandes usando una abstracción de paginación,
- medir el comportamiento del sistema mediante `page hits` y `page faults`,
- producir tanto una salida binaria ordenada como una versión legible en texto.

---

## Estructura del proyecto

El repositorio contiene dos ejecutables principales:

- `generator`: crea archivos binarios con números aleatorios.
- `sorter`: ordena el archivo binario usando `PagedArray`.

---

## Requisitos

Para compilar este proyecto se utiliza:

- C++17
- Visual Studio 2022 o g++
- Biblioteca estándar de C++

---

## Compilación

### Con g++
```bash
g++ -O2 -std=c++17 generator.cpp -o generator
g++ -O2 -std=c++17 sorter.cpp -o sorter
