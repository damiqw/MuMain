// <copyright file="MuEmuSettings.cs" company="MUnique">
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// </copyright>

namespace MUnique.Client.Library.MuEmu;

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using MUnique.OpenMU.Network.SimpleModulus;

/// <summary>
/// Settings to connect to a MuEmu based game server, read from the <c>[MuEmu]</c> section of <c>config.ini</c>.
/// </summary>
/// <remarks>
/// MuEmu game servers wrap the whole TCP stream in an additional byte-wise XOR layer and may use their own
/// SimpleModulus keys. Without this section (or with <c>Enabled = 0</c>) the client behaves exactly like upstream.
/// </remarks>
internal sealed class MuEmuSettings
{
    private const string SectionName = "MuEmu";

    private const int CustomerNameLength = 32;

    private const int ServerSerialLength = 17;

    private static readonly Lazy<MuEmuSettings> LazyCurrent = new(Load);

    private MuEmuSettings()
    {
    }

    /// <summary>
    /// Gets the settings of the current client installation.
    /// </summary>
    public static MuEmuSettings Current => LazyCurrent.Value;

    /// <summary>
    /// Gets a value indicating whether the MuEmu protocol adjustments are enabled.
    /// </summary>
    public bool Enabled { get; private set; }

    /// <summary>
    /// Gets the keys of the stream XOR layer, or <c>null</c> if the layer is disabled.
    /// </summary>
    public MuEmuXorKeys? XorKeys { get; private set; }

    /// <summary>
    /// Gets the SimpleModulus keys to encrypt client-to-server packets, or <c>null</c> to use the default keys.
    /// </summary>
    public SimpleModulusKeys? EncryptionKeys { get; private set; }

    /// <summary>
    /// Gets the SimpleModulus keys to decrypt server-to-client packets, or <c>null</c> to use the default keys.
    /// </summary>
    public SimpleModulusKeys? DecryptionKeys { get; private set; }

    /// <summary>
    /// Calculates the XOR layer keys the same way the MuEmu game server and Main.dll do.
    /// </summary>
    /// <param name="customerName">The customer name of the server configuration.</param>
    /// <param name="serverSerial">The server serial of the server configuration.</param>
    /// <returns>The first and the second key.</returns>
    public static (byte Key1, byte Key2) CalculateXorKeys(string customerName, string serverSerial)
    {
        var name = ToFixedBytes(customerName, CustomerNameLength);
        var serial = ToFixedBytes(serverSerial, ServerSerialLength);

        ushort sum = 0;
        for (var n = 0; n < CustomerNameLength; n++)
        {
            sum += (byte)(name[n] ^ serial[n % ServerSerialLength]);
        }

        return ((byte)(0xEE + (sum & 0xFF)), (byte)(0xFF + (sum >> 8)));
    }

    private static MuEmuSettings Load()
    {
        var settings = new MuEmuSettings();
        try
        {
            var path = Path.Combine(AppContext.BaseDirectory, "config.ini");
            if (!File.Exists(path))
            {
                return settings;
            }

            var values = ReadSection(path, SectionName);
            settings.Enabled = GetInt(values, "Enabled") == 1;
            if (!settings.Enabled)
            {
                return settings;
            }

            settings.XorKeys = ReadXorKeys(values);
            settings.EncryptionKeys = ReadKeyFile(values, "EncryptKeyFile", isEncryption: true);
            settings.DecryptionKeys = ReadKeyFile(values, "DecryptKeyFile", isEncryption: false);

            ManagedLog.Write(
                ManagedLog.Level.Info,
                $"NET: MuEmu protocol enabled (xor layer={(settings.XorKeys is null ? "off" : "on")}, encrypt keys={(settings.EncryptionKeys is null ? "default" : "file")}, decrypt keys={(settings.DecryptionKeys is null ? "default" : "file")})");
        }
        catch (Exception ex)
        {
            ManagedLog.Write(ManagedLog.Level.Error, $"NET: Failed to read MuEmu settings: {ex}");
        }

        return settings;
    }

    private static MuEmuXorKeys? ReadXorKeys(Dictionary<string, string> values)
    {
        byte key1;
        byte key2;
        if (values.ContainsKey("XorKey1") && values.ContainsKey("XorKey2"))
        {
            key1 = (byte)GetInt(values, "XorKey1");
            key2 = (byte)GetInt(values, "XorKey2");
        }
        else if (values.TryGetValue("CustomerName", out var customerName) && values.TryGetValue("ServerSerial", out var serverSerial))
        {
            (key1, key2) = CalculateXorKeys(customerName, serverSerial);
        }
        else
        {
            return null;
        }

        // Optional second layer (MHP), configured with its final key values.
        var mhpKey1 = (byte)GetInt(values, "MhpXorKey1");
        var mhpKey2 = (byte)GetInt(values, "MhpXorKey2");
        return new MuEmuXorKeys(key1, key2, mhpKey1, mhpKey2);
    }

    private static SimpleModulusKeys? ReadKeyFile(Dictionary<string, string> values, string key, bool isEncryption)
    {
        if (!values.TryGetValue(key, out var fileName) || string.IsNullOrWhiteSpace(fileName))
        {
            return null;
        }

        var path = Path.IsPathRooted(fileName) ? fileName : Path.Combine(AppContext.BaseDirectory, fileName);
        if (!File.Exists(path))
        {
            ManagedLog.Write(ManagedLog.Level.Error, $"NET: MuEmu key file not found: {path}");
            return null;
        }

        var serializer = new SimpleModulusKeySerializer();
        if (!serializer.TryDeserialize(path, out var modulusKey, out var cryptKey, out var xorKey))
        {
            ManagedLog.Write(ManagedLog.Level.Error, $"NET: MuEmu key file is invalid: {path}");
            return null;
        }

        return isEncryption
            ? new SimpleModulusKeys(modulusKey, xorKey, cryptKey, new uint[4])
            : new SimpleModulusKeys(modulusKey, xorKey, new uint[4], cryptKey);
    }

    private static byte[] ToFixedBytes(string value, int length)
    {
        var result = new byte[length];
        var bytes = Encoding.Latin1.GetBytes(value);
        Array.Copy(bytes, result, Math.Min(bytes.Length, length - 1));
        return result;
    }

    private static int GetInt(Dictionary<string, string> values, string key)
    {
        if (!values.TryGetValue(key, out var text))
        {
            return 0;
        }

        text = text.Trim();
        if (text.StartsWith("0x", StringComparison.OrdinalIgnoreCase)
            && int.TryParse(text.AsSpan(2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var hex))
        {
            return hex;
        }

        return int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var number) ? number : 0;
    }

    private static Dictionary<string, string> ReadSection(string path, string section)
    {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var inSection = false;
        foreach (var rawLine in File.ReadAllLines(path))
        {
            var line = rawLine.Trim();
            if (line.Length == 0 || line[0] == ';' || line[0] == '#')
            {
                continue;
            }

            if (line[0] == '[')
            {
                inSection = string.Equals(line.Trim('[', ']').Trim(), section, StringComparison.OrdinalIgnoreCase);
                continue;
            }

            var separator = line.IndexOf('=');
            if (inSection && separator > 0)
            {
                result[line[..separator].Trim()] = line[(separator + 1)..].Trim();
            }
        }

        return result;
    }
}
