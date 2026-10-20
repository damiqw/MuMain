#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Ventanas de las funciones custom del servidor (las de Main.dll): eventos, comandos, banco de joyas, baul,
// rankings, medidor de dano, recompensa diaria, recompensas de guild, invasiones y busqueda de party.
// Tecla N: menu MuEmu; tecla H: horarios de eventos (como en Main.dll).
namespace MuEmu::Ui
{
// Paquetes del servidor que maneja MuEmu; verdadero si el paquete era suyo.
bool Receive(const uint8_t* buffer, int size);

// Teclas de las ventanas; verdadero si la tecla se uso.
bool HandleKeys();

// Cada frame de la escena del juego, en la pasada 2D: textos sobre los personajes y relojes.
void Frame();

// Menu con todas las ventanas.
void ShowMenu();

// --- Ayudas para los modulos de cada ventana ---

// Envia un paquete C1 (cabecera, subcodigo y datos).
void Send(uint8_t head, uint8_t sub, const std::vector<uint8_t>& payload = {});

int32_t ReadInt(const uint8_t* buffer, int offset);
// Texto del servidor (ANSI o UTF-8) de hasta `length` bytes, en UTF-8.
std::string ReadText(const uint8_t* buffer, int offset, int length);

void WriteInt(std::vector<uint8_t>& payload, int32_t value);

// "1.234.567"
std::string FormatNumber(int64_t value);
// "hh:mm:ss" (o "d h" si pasa de un dia)
std::string FormatTime(int seconds);
} // namespace MuEmu::Ui
