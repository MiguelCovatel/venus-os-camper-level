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

Pequeñas milésimas de grado son ruido normal. Un inicio razonable es una variación de `0,10°`
durante `3 s`; después puede ajustarse con datos reales de la camper.

