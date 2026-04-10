# Proyecto 1 - Arreglos Paginados

Este repositorio contiene dos programas independientes: uno para generar archivos binarios con números enteros aleatorios (`generator`) y otro para ordenarlos usando una clase que simula paginación (`sorter`).  

## Proyectos que contiene

- **Generator**: crea un archivo binario de un tamaño fijo (SMALL, MEDIUM o LARGE). Cada número ocupa exactamente 4 bytes, sin comas ni separadores.  
- **Sorter**: toma ese archivo binario, lo ordena con alguno de los algoritmos implementados (merge, counting, radix, quick, bucket) y genera dos archivos de salida: uno binario ordenado y otro legible (ASCII con comas).  

## ¿Cómo se compila?

Abre el archivo `.sln` en Visual Studio. Selecciona la configuración **Release** (así el código corre más rápido). Luego compila cada proyecto por separado o compila toda la solución. Los ejecutables quedan en carpetas como `x64\Release\` (o `Debug` si usas esa configuración).

## Ejecutar desde Visual Studio (IDE)

No necesitas buscar los `.exe`; puedes correr los programas directamente desde el IDE. Solo tienes que indicarle a Visual Studio qué programa quieres ejecutar y qué parámetros debe usar.

### Para el generador (`GeneratorProject`)

1. En el **Explorador de soluciones**, haz clic derecho sobre `GeneratorProject` y selecciona **Establecer como proyecto de inicio**.  
2. Ve a las propiedades del proyecto (clic derecho → **Propiedades**).  
3. Navega a **Propiedades de configuración** → **Depuración**.  
4. En la línea **Argumentos de comando**, escribe algo como:  
   `-size <TAMAÑO> -output <archivo>.bin`
   En <TAMAÑO> hay tres opciones
   1 - SMALL: 128MB
   2 - MEDIUM: 256MB
   3 - LARGE: 512MB
   (El archivo se guardará en la misma carpeta que el `.exe`, que suele ser `x64\Release` o `x64\Debug`).  
6. Presiona `F5` o `Ctrl+F5` para ejecutar.

### Para el ordenador (`SorterProject`)

1. Cambia el proyecto de inicio: clic derecho sobre `SorterProject` → **Establecer como proyecto de inicio**.  
2. Otra vez, ve a sus propiedades → **Depuración**.  
3. En **Argumentos de comando** pon algo como:  
   `-input ../GeneratorProject/datos.bin -output ordenado.bin -alg quick -pageSize 16384 -pageCount 128`  

   Explicación rápida:  
   - `-input` apunta al archivo que generó el `generator`. Como los dos proyectos están en la misma solución, puedes subir una carpeta con `..` para encontrar el archivo.  
   - `-output` es el nombre del archivo ordenado (se guardará en la carpeta de `SorterProject`).  
   - `-alg` elige el algoritmo: `merge`, `counting`, `radix`, `quick` o `bucket`.  
   - `-pageSize` es el tamaño de cada página **en bytes**. Como cada entero ocupa 4 bytes, este número debe ser múltiplo de 4.  
   - `-pageCount` es cuántas páginas se pueden tener en RAM al mismo tiempo.  
4. Ejecuta con `F5`.


## Ejecutar desde la terminal (CMD o PowerShell)

Primero tienes que ubicarte en la carpeta donde están los `.exe`. Por ejemplo, si compilaste en Release de 64 bits:

```bash
cd "C:...\Proyecto-1-Datos-2\x64\Release"
```
o por debug
```bash
cd "C:...\Proyecto-1-Datos-2\x64\Debug"
```

Luego desde ahí se ejecuta `Generator.exe -size <SMALL/MEDIUM/LARGE> -output <archivo>.bin `
Para el generator y 
`Sorter.exe -input <archivo>.bin -output <ordenado>.bin -alg <algoritmo> -pageSize <tamaño> -pageCount <paginas>`
