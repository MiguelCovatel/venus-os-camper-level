# Calibración y uso

## Medidas

- **Batalla:** centro del eje delantero al centro del trasero.
- **Vía delantera:** centros de las ruedas delanteras.
- **Vía trasera:** centros de las ruedas traseras.

Mide en milímetros. No uses la anchura exterior de neumáticos.

## Orientación

Inclina ligeramente la parte delantera y después el lado izquierdo. Comprueba qué signo cambia en
pitch/roll. Corrige con `swap_axes`, `invert_pitch` e `invert_roll`, sin cambiar el código.

## Nivel 0

1. Nivela con un nivel de confianza situado en suelo/chasis.
2. Detén motor y movimientos dentro.
3. Espera a `STABLE`.
4. Pulsa **Establecer nivel 0** una vez.
5. Espera la confirmación del ESP32.

El cero compensa la inclinación de montaje. No es necesario introducir altura o posición
longitudinal del MPU para medir un plano rígido en reposo.

## Sensibilidad práctica

El perfil predeterminado **Burbuja camper** considera nivelado hasta `±0,5°`, aceptable hasta
`±1,0°` y estable cuando la variación permanece por debajo de `0,20°` durante `3 s`.

La interfaz muestra pitch/roll con una décima y las ruedas en intervalos de `10 mm`. No se pierde
la precisión interna: simplemente se evita convertir ruido, flexión del suelo o movimiento de la
suspensión en instrucciones imposibles de ejecutar con una rampa real.

Desde la web se puede elegir **Precisa**, **Burbuja camper** o **Relajada**. Para uso normal se
recomienda conservar el perfil de burbuja y afinarlo únicamente con experiencia real en el vehículo.
