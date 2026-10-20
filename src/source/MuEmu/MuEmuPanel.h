#pragma once

#include <functional>
#include <string>
#include <vector>

// La ventana compartida de las funciones custom del servidor (eventos, comandos, banco de joyas,
// rankings, recompensas...): un titulo, lineas de texto, una tabla y botones (muemu_panel.rml).
// Se muestra una a la vez; abrir otra reemplaza a la anterior.
namespace MuEmu::Panel
{
struct Button
{
    std::string label;
    int action = 0;
};

struct Row
{
    std::vector<std::string> cells;
    std::string color; // "#rrggbb", vacio es blanco
    std::vector<Button> buttons;
};

struct Content
{
    std::string title;
    std::vector<std::string> lines;
    std::vector<std::string> header;
    std::vector<Row> rows;
    std::vector<Button> buttons;
    bool wide = false; // ventana ancha (tablas de varias columnas)
    std::string inputLabel; // si no esta vacio, un campo de texto con este titulo
    std::string inputValue;
};

using ActionHandler = std::function<void(int action)>;

// Abre la ventana `id` (reemplaza la que este abierta).
void Show(const std::string& id, Content content, ActionHandler onAction);

// Cambia el contenido si la ventana `id` esta abierta.
void Update(const std::string& id, Content content);

bool IsShown(const std::string& id);

// Si hay alguna ventana MuEmu abierta (ESC la cierra).
bool IsAnyShown();

// Lo escrito en el campo de texto de la ventana.
std::string GetInput();

void Hide();

// Texto UTF-8 de un texto del cliente.
std::string ToUtf8(const wchar_t* text);
} // namespace MuEmu::Panel
