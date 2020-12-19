// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

namespace WixToolset.Core.TestPackage
{
    using System;
    using System.Collections.Generic;
    using WixToolset.Data;
    using Xunit;

    /// <summary>
    /// The result of an Execute method of <see cref="WixRunner"/>.
    /// </summary>
    public class WixRunnerResult
    {
        /// <summary>
        /// ExitCode for the operation.
        /// </summary>
        public int ExitCode { get; set; }

        /// <summary>
        /// Messages from the operation.
        /// </summary>
        public Message[] Messages { get; set; }

        /// <summary>
        /// 
        /// </summary>
        /// <returns></returns>
        public WixRunnerResult AssertSuccess()
        {
            Assert.True(0 == this.ExitCode, $"\r\n\r\nWixRunner failed with exit code: {this.ExitCode}\r\n   Output: {String.Join("\r\n           ", FormatMessages(this.Messages))}\r\n");
            return this;
        }

        private static IEnumerable<string> FormatMessages(IEnumerable<Message> messages)
        {
            foreach (var message in messages)
            {
                var filename = message.SourceLineNumbers?.FileName ?? "TEST";
                var line = message.SourceLineNumbers?.LineNumber ?? -1;
                var type = message.Level.ToString().ToLowerInvariant();

                if (line > 0)
                {
                    filename = String.Concat(filename, "(", line, ")");
                }

                yield return String.Format("{0} : {1} {2}{3:0000}: {4}", filename, type, "TEST", message.Id, message.ToString());
            }
        }
    }
}
