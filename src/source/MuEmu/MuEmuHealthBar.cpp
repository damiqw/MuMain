#include "stdafx.h"
#include "MuEmu/MuEmuHealthBar.h"
#include "MuEmu/MuEmuTools.h"

#include "Camera/CameraProjection.h"
#include "Camera/CameraState.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzInventory.h"
#include "Engine/Object/ZzzObject.h"
#include "Render/Text/CUIRenderText.h"
#include "Render/Textures/ZzzOpenglUtil.h"

#include <vector>

namespace
{
// Tipos del servidor (OBJECT_USER / OBJECT_MONSTER).
constexpr BYTE ObjectUser = 1;
constexpr BYTE ObjectMonster = 2;

// Cabecera C2 F3 E2: tipo, tamano (2), codigo, subcodigo, cantidad y flag del servidor (FLAG = 1).
constexpr int HeaderSize = 7;
// Entrada: indice (WORD, little endian), tipo, % de vida, % de SD y flag.
constexpr int EntrySize = 6;

struct Entry
{
    int key = 0;
    BYTE type = 0;
    BYTE life = 0;
    BYTE shield = 0;
};

std::vector<Entry> g_entries;

bool IsEnabled(const char* key)
{
    return MuEmu::GetConfigInt(key, 1) != 0;
}

// Fondo oscuro y barra de color en el espacio de referencia, como pDrawBarForm en Main.dll.
void DrawBar(float x, float y, float width, float height, float rate, unsigned int color)
{
    EnableAlphaBlend();
    RenderColorQuadARGB(x - 1.f, y - 1.f, width + 2.f, height + 2.f, 0x99000000u);
    RenderColorQuadARGB(x, y, width * (rate / 100.f), height, color);
    DisableAlphaBlend();
}
} // namespace

void MuEmu::HealthBar::Receive(const BYTE* buffer, int size)
{
    g_entries.clear();
    if (buffer == nullptr || size < HeaderSize)
    {
        return;
    }

    const int count = buffer[5];
    for (int n = 0; n < count; ++n)
    {
        const BYTE* entry = buffer + HeaderSize + (n * EntrySize);
        if (entry + EntrySize > buffer + size)
        {
            break;
        }

        Entry info;
        info.key = entry[0] | (entry[1] << 8);
        info.type = entry[2];
        info.life = entry[3] > 100 ? 100 : entry[3];
        info.shield = entry[4] > 100 ? 100 : entry[4];
        g_entries.push_back(info);
    }
}

void MuEmu::HealthBar::Render()
{
    static const bool monsterBars = IsEnabled("MonsterHpBar");
    static const bool userBars = IsEnabled("UserHpBar");

    for (const Entry& entry : g_entries)
    {
        if ((entry.type == ObjectMonster && !monsterBars) || (entry.type == ObjectUser && !userBars))
        {
            continue;
        }

        const int index = FindCharacterIndex(entry.key);
        if (index >= MAX_CHARACTERS_CLIENT)
        {
            continue;
        }

        CHARACTER* c = &CharactersClient[index];
        OBJECT* o = &c->Object;
        if (!o->Live || !o->Visible || o->Alpha <= 0.f || c->Dead > 0 || c == Hero)
        {
            continue;
        }

        // Los jugadores solo muestran la barra cuando estan seleccionados, como en Main.dll.
        if (entry.type == ObjectUser && index != SelectedCharacter)
        {
            continue;
        }

        vec3_t position;
        Vector(o->Position[0], o->Position[1], o->Position[2] + o->BoundingBoxMax[2] + 100.f, position);

        vec3_t transformed;
        VectorTransform(position, g_Camera.Matrix, transformed);
        if (transformed[2] >= 0)
        {
            continue;
        }

        int screenX = 0;
        int screenY = 0;
        CameraProjection::WorldToScreen(g_Camera, position, &screenX, &screenY);
        if (screenX < -100 || screenY < -100 || screenX > (REFERENCE_WIDTH + 100) || screenY > (REFERENCE_HEIGHT + 100))
        {
            continue;
        }

        constexpr float BarWidth = 40.f;
        const float x = static_cast<float>(screenX) - (BarWidth / 2.f);
        const float y = static_cast<float>(screenY);

        if (entry.type == ObjectMonster)
        {
            DrawBar(x, y, BarWidth, 2.5f, entry.life, 0xCCFF0000u);

            g_pRenderText->SetTextColor(255, 255, 255, 255);
            g_pRenderText->SetBgColor(0, 0, 0, 0);
            g_pRenderText->RenderText(screenX, static_cast<int>(y) - 10, c->ID, 0, 0, RT3_WRITE_CENTER);
        }
        else
        {
            // Vida verde (azul si estamos en party) y SD amarillo debajo.
            DrawBar(x, y - 4.f, BarWidth, 3.f, entry.life, PartyNumber > 0 ? 0xCC0000FFu : 0xCC00FF00u);
            DrawBar(x, y + 1.f, BarWidth, 2.5f, entry.shield, 0xCCFFFF00u);
        }
    }
}
