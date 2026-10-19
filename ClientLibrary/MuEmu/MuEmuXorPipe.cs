// <copyright file="MuEmuXorPipe.cs" company="MUnique">
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// </copyright>

namespace MUnique.Client.Library.MuEmu;

using System;
using System.IO.Pipelines;
using System.Threading.Tasks;

/// <summary>
/// Applies the MuEmu stream XOR layer between the socket and the packet encryption.
/// </summary>
internal static class MuEmuXorPipe
{
    private delegate void Transform(Span<byte> data);

    /// <summary>
    /// Wraps the socket input, so that the returned reader provides the bytes without the XOR layer.
    /// </summary>
    /// <param name="socketInput">The raw socket input.</param>
    /// <param name="keys">The keys.</param>
    /// <returns>The reader with the decrypted bytes.</returns>
    public static PipeReader WrapInput(PipeReader socketInput, MuEmuXorKeys keys)
    {
        var pipe = new Pipe();
        _ = PumpAsync(socketInput, pipe.Writer, keys.Decrypt);
        return pipe.Reader;
    }

    /// <summary>
    /// Wraps the socket output, so that everything written to the returned writer gets the XOR layer applied.
    /// </summary>
    /// <param name="socketOutput">The raw socket output.</param>
    /// <param name="keys">The keys.</param>
    /// <returns>The writer which encrypts the bytes.</returns>
    public static PipeWriter WrapOutput(PipeWriter socketOutput, MuEmuXorKeys keys)
    {
        var pipe = new Pipe();
        _ = PumpAsync(pipe.Reader, socketOutput, keys.Encrypt);
        return pipe.Writer;
    }

    private static async Task PumpAsync(PipeReader reader, PipeWriter writer, Transform transform)
    {
        Exception? error = null;
        try
        {
            while (true)
            {
                var result = await reader.ReadAsync().ConfigureAwait(false);
                var buffer = result.Buffer;
                foreach (var segment in buffer)
                {
                    var target = writer.GetSpan(segment.Length)[..segment.Length];
                    segment.Span.CopyTo(target);
                    transform(target);
                    writer.Advance(segment.Length);
                }

                reader.AdvanceTo(buffer.End);
                var flushResult = await writer.FlushAsync().ConfigureAwait(false);
                if (result.IsCompleted || result.IsCanceled || flushResult.IsCompleted || flushResult.IsCanceled)
                {
                    break;
                }
            }
        }
        catch (Exception ex)
        {
            error = ex;
            ManagedLog.Write(ManagedLog.Level.Debug, $"NET: MuEmu XOR pump ended: {ex.Message}");
        }

        await reader.CompleteAsync(error).ConfigureAwait(false);
        await writer.CompleteAsync(error).ConfigureAwait(false);
    }
}
