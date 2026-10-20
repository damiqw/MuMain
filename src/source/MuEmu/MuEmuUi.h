#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Ventanas de las funciones custom del servidor (las de Main.dll): eventos, comandos, banco de joyas, baul,
// rankings, medidor de dano, recompensa diaria, recompensas de guild, invasiones y busqueda de party.
// Como en Main.dll: N party, H horarios de eventos, J banco de joyas y el boton Menu de la pantalla para el resto.
namespace MuEmu::Ui
{
// Paquetes del servidor que maneja MuEmu; verdadero si el paquete era suyo.
bool Receive(const uint8_t* buffer, int size);

// Teclas de las ventanas; verdadero si la tecla se uso.
bool HandleKeys();

// Cada frame de la escena del juego, en la pasada 2D: textos sobre los personajes y relojes.
void Frame();

// El menu del boton Menu (eventos, comandos, rankings, recompensas...); si esta abierto lo cierra.
void ShowMenu();

// La ventana de la N: publicar o quitar la party y buscar partys.
void ShowPartyMenu();

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
