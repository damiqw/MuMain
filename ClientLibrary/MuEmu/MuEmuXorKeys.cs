// <copyright file="MuEmuXorKeys.cs" company="MUnique">
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// </copyright>

namespace MUnique.Client.Library.MuEmu;

using System;

/// <summary>
/// Keys of the byte-wise XOR layer which MuEmu game servers apply on the whole TCP stream.
/// </summary>
/// <remarks>
/// Server and Main.dll use the same functions:
/// encrypt: <c>b = (b + Key2) ^ Key1</c>, then the optional MHP layer <c>b = (b + MhpKey2) ^ MhpKey1</c>;
/// decrypt: the MHP layer first <c>b = (b ^ MhpKey1) - MhpKey2</c>, then <c>b = (b ^ Key1) - Key2</c>.
/// The MHP layer is only active when one of its keys is not zero.
/// </remarks>
/// <param name="Key1">The first key.</param>
/// <param name="Key2">The second key.</param>
/// <param name="MhpKey1">The first key of the optional MHP layer.</param>
/// <param name="MhpKey2">The second key of the optional MHP layer.</param>
internal sealed record MuEmuXorKeys(byte Key1, byte Key2, byte MhpKey1, byte MhpKey2)
{
    private bool HasMhpLayer => this.MhpKey1 != 0 || this.MhpKey2 != 0;

    /// <summary>
    /// Encrypts outgoing bytes in place.
    /// </summary>
    /// <param name="data">The data.</param>
    public void Encrypt(Span<byte> data)
    {
        for (var i = 0; i < data.Length; i++)
        {
            data[i] = (byte)((data[i] + this.Key2) ^ this.Key1);
        }

        if (this.HasMhpLayer)
        {
            for (var i = 0; i < data.Length; i++)
            {
                data[i] = (byte)((data[i] + this.MhpKey2) ^ this.MhpKey1);
            }
        }
    }

    /// <summary>
    /// Decrypts incoming bytes in place.
    /// </summary>
    /// <param name="data">The data.</param>
    public void Decrypt(Span<byte> data)
    {
        if (this.HasMhpLayer)
        {
            for (var i = 0; i < data.Length; i++)
            {
                data[i] = (byte)((data[i] ^ this.MhpKey1) - this.MhpKey2);
            }
        }

        for (var i = 0; i < data.Length; i++)
        {
            data[i] = (byte)((data[i] ^ this.Key1) - this.Key2);
        }
    }
}
