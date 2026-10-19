// <copyright file="MuEmuCrypto.cs" company="MUnique">
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// </copyright>

namespace MUnique.Client.Library.MuEmu;

using System.IO.Pipelines;
using MUnique.OpenMU.Network;
using MUnique.OpenMU.Network.SimpleModulus;
using MUnique.OpenMU.Network.Xor;

/// <summary>
/// Creates the encryption pipeline of a game server connection, with or without the MuEmu adjustments.
/// </summary>
internal static class MuEmuCrypto
{
    /// <summary>
    /// Creates the encryptor and decryptor for a connection.
    /// </summary>
    /// <param name="socketConnection">The raw socket connection.</param>
    /// <param name="isEncrypted">If set to <c>true</c>, the connection is encrypted (game server).</param>
    /// <returns>The encryptor and decryptor, or <c>null</c> for an unencrypted connection.</returns>
    public static (IPipelinedEncryptor? Encryptor, IPipelinedDecryptor? Decryptor) Create(IDuplexPipe socketConnection, bool isEncrypted)
    {
        if (!isEncrypted)
        {
            return (null, null);
        }

        var settings = MuEmuSettings.Current;
        if (!settings.Enabled)
        {
            return (
                new PipelinedXor32Encryptor(new PipelinedSimpleModulusEncryptor(socketConnection.Output, PipelinedSimpleModulusEncryptor.DefaultClientKey).Writer),
                new PipelinedSimpleModulusDecryptor(socketConnection.Input, PipelinedSimpleModulusDecryptor.DefaultClientKey));
        }

        var output = socketConnection.Output;
        var input = socketConnection.Input;
        if (settings.XorKeys is { } xorKeys)
        {
            output = MuEmuXorPipe.WrapOutput(output, xorKeys);
            input = MuEmuXorPipe.WrapInput(input, xorKeys);
        }

        var simpleModulusEncryptor = settings.EncryptionKeys is { } encryptionKeys
            ? new PipelinedSimpleModulusEncryptor(output, encryptionKeys)
            : new PipelinedSimpleModulusEncryptor(output, PipelinedSimpleModulusEncryptor.DefaultClientKey);
        var decryptor = settings.DecryptionKeys is { } decryptionKeys
            ? new PipelinedSimpleModulusDecryptor(input, decryptionKeys)
            : new PipelinedSimpleModulusDecryptor(input, PipelinedSimpleModulusDecryptor.DefaultClientKey);

        return (new PipelinedXor32Encryptor(simpleModulusEncryptor.Writer), decryptor);
    }
}
