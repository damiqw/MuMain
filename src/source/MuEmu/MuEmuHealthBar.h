#pragma once

// Barras de vida de los servidores MuEmu (paquete C2 F3 E2), como las dibuja Main.dll:
// barra roja con nombre sobre cada monstruo y, sobre el jugador seleccionado, vida (verde, azul en party)
// y SD (amarillo). Se configuran en config.ini, seccion [MuEmu]: MonsterHpBar y UserHpBar (1 por defecto).
namespace MuEmu::HealthBar
{
// Guarda la lista que envia el servidor (reemplaza la anterior).
void Receive(const BYTE* buffer, int size);

// Dibuja las barras. Se llama con la pantalla de referencia (640x480) ya preparada.
void Render();
} // namespace MuEmu::HealthBar
