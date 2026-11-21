using System.Text.Json;
using TFTape.Ui.Models;
using TFTape.Ui.Services;
using TFTape.Ui.ViewModels;
using Xunit;

namespace TFTape.Ui.Tests;

public sealed class VodLibraryTests
{
    private const string SidecarJson = """
        {"schemaVersion":1,"videoFile":"jogo.mp4","createdAtUtc":"2026-11-20T22:40:12Z",
         "durationMs":93210,"resolution":"1080p","fps":60,
         "rounds":[{"label":"2-1","startMs":18420,"source":"hud-roi","confidence":0.91}]}
        """;

    [Fact]
    public async Task ScanFindsMp4WithSidecarMetadata()
    {
        var dir = NewTempDir();
        try
        {
            var mp4 = Path.Combine(dir, "jogo.mp4");
            File.WriteAllBytes(mp4, new byte[] { 1, 2, 3 });
            File.WriteAllText(mp4 + ".tftape.json", SidecarJson);

            var found = await new VodLibraryService().ScanAsync(dir);

            var vod = Assert.Single(found);
            Assert.True(vod.HasSidecar);
            Assert.Equal("jogo.mp4", vod.FileName);
            Assert.Equal(mp4 + ".tftape.json", vod.SidecarPath);
            Assert.Equal(93210L, vod.DurationMs);
            Assert.Equal("1080p", vod.Resolution);
            Assert.Equal(60, vod.Fps);
            var round = Assert.Single(vod.Rounds);
            Assert.Equal("2-1", round.Label);
            Assert.Equal(18420L, round.StartMs);
            Assert.Equal(string.Empty, vod.LoadError);
        }
        finally { Directory.Delete(dir, recursive: true); }
    }

    [Fact]
    public async Task ScanListsMp4EvenWithoutSidecar()
    {
        var dir = NewTempDir();
        try
        {
            File.WriteAllBytes(Path.Combine(dir, "sem_meta.mp4"), new byte[] { 1 });

            var vod = Assert.Single(await new VodLibraryService().ScanAsync(dir));

            Assert.False(vod.HasSidecar);
            Assert.Empty(vod.Rounds);
            Assert.Equal(string.Empty, vod.LoadError);
        }
        finally { Directory.Delete(dir, recursive: true); }
    }

    [Fact]
    public async Task ScanKeepsMp4WhenSidecarIsCorrupt()
    {
        var dir = NewTempDir();
        try
        {
            var mp4 = Path.Combine(dir, "quebrado.mp4");
            File.WriteAllBytes(mp4, new byte[] { 1 });
            File.WriteAllText(mp4 + ".tftape.json", "{ isto nao eh json");

            var vod = Assert.Single(await new VodLibraryService().ScanAsync(dir));

            Assert.False(vod.HasSidecar);
            Assert.Contains("Metadados inválidos", vod.LoadError);
        }
        finally { Directory.Delete(dir, recursive: true); }
    }

    [Fact]
    public async Task ScanSortsNewestFirst()
    {
        var dir = NewTempDir();
        try
        {
            WriteVod(dir, "antigo.mp4", "2026-11-18T10:00:00Z");
            WriteVod(dir, "novo.mp4", "2026-11-20T10:00:00Z");

            var found = await new VodLibraryService().ScanAsync(dir);

            Assert.Equal("novo.mp4", found[0].FileName);
            Assert.Equal("antigo.mp4", found[1].FileName);
        }
        finally { Directory.Delete(dir, recursive: true); }
    }

    [Fact]
    public async Task ScanReturnsEmptyForMissingFolder()
    {
        var found = await new VodLibraryService().ScanAsync(Path.Combine(Path.GetTempPath(), "tftape_inexistente"));
        Assert.Empty(found);
    }

    [Fact]
    public async Task ScanIgnoresNonMp4Files()
    {
        var dir = NewTempDir();
        try
        {
            WriteVod(dir, "jogo.mp4", "2026-11-20T10:00:00Z");
            File.WriteAllText(Path.Combine(dir, "notas.txt"), "oi");

            Assert.Single(await new VodLibraryService().ScanAsync(dir));
        }
        finally { Directory.Delete(dir, recursive: true); }
    }

    [Fact]
    public void SidecarParseIgnoresUnknownFields()
    {
        const string json = """
            {"schemaVersion":1,"videoFile":"a.mp4","createdAtUtc":"2026-11-20T22:40:12Z",
             "durationMs":1000,"resolution":"720p","fps":30,"rounds":[],"campoFuturo":123}
            """;

        var sidecar = RecordingSidecar.Parse(json);

        Assert.Equal(1, sidecar.SchemaVersion);
        Assert.Equal("a.mp4", sidecar.VideoFile);
        Assert.Empty(sidecar.Rounds);
    }

    [Fact]
    public void SidecarParseRejectsInvalidSchema()
    {
        Assert.Throws<JsonException>(() => RecordingSidecar.Parse("""{"schemaVersion":0,"rounds":[]}"""));
        Assert.Throws<JsonException>(() => RecordingSidecar.Parse("   "));
    }

    [Fact]
    public async Task ViewModelPublishesPlayAndSeekRequests()
    {
        var round = new RoundMarker { Label = "2-1", StartMs = 18420 };
        var vod = new VodEntry
        {
            FilePath = @"C:\Vods\jogo.mp4",
            DurationMs = 93210,
            Rounds = new[] { round }
        };
        var library = new FakeVodLibraryService { Entries = new[] { vod } };
        var vm = new VodLibraryViewModel(library, new InMemoryOutputDirectoryStore(@"C:\Vods"));
        var played = new List<string>();
        var seeks = new List<long>();
        vm.PlayRequested += played.Add;
        vm.SeekRequested += seeks.Add;

        await vm.RefreshAsync();
        vm.JumpToRound(round);

        Assert.Equal(new[] { @"C:\Vods\jogo.mp4" }, played);
        Assert.Equal(new[] { 18420L }, seeks);
    }

    [Fact]
    public async Task ViewModelReportsEmptyFolderAndUpdatesStatus()
    {
        var vm = new VodLibraryViewModel(new FakeVodLibraryService(),
            new InMemoryOutputDirectoryStore(@"C:\Vods"));

        await vm.RefreshAsync();

        Assert.Equal("Nenhum VOD encontrado nesta pasta.", vm.Status);
        Assert.Null(vm.SelectedVod);
        Assert.Equal("Selecione um VOD para começar.", vm.RoundsHint);
    }

    [Fact]
    public void ViewModelFallsBackToRecordingFolderAndPersistsChanges()
    {
        var store = new InMemoryOutputDirectoryStore();
        var vm = new VodLibraryViewModel(new FakeVodLibraryService(), store,
            new InMemoryOutputDirectoryStore(@"C:\Gravacoes"));

        Assert.Equal(@"C:\Gravacoes", vm.Folder);

        vm.Folder = @"D:\Vods";

        Assert.Equal(@"D:\Vods", store.Load());
        Assert.Equal(@"D:\Vods", new VodLibraryViewModel(new FakeVodLibraryService(), store).Folder);
    }

    [Fact]
    public async Task ViewModelRejectsRoundBeyondDuration()
    {
        var round = new RoundMarker { Label = "9-9", StartMs = 999999 };
        var vod = new VodEntry { FilePath = @"C:\Vods\curto.mp4", DurationMs = 1000, Rounds = new[] { round } };
        var vm = new VodLibraryViewModel(new FakeVodLibraryService { Entries = new[] { vod } },
            new InMemoryOutputDirectoryStore(@"C:\Vods"));
        var seeks = new List<long>();
        vm.SeekRequested += seeks.Add;
        await vm.RefreshAsync();

        vm.JumpToRound(round);

        Assert.Empty(seeks);
    }

    private static void WriteVod(string dir, string name, string createdAtUtc)
    {
        var mp4 = Path.Combine(dir, name);
        File.WriteAllBytes(mp4, new byte[] { 1 });
        File.WriteAllText(mp4 + ".tftape.json",
            $"{{\"schemaVersion\":1,\"videoFile\":\"{name}\",\"createdAtUtc\":\"{createdAtUtc}\"," +
            "\"durationMs\":1000,\"resolution\":\"720p\",\"fps\":30,\"rounds\":[]}");
    }

    private static string NewTempDir()
    {
        var dir = Path.Combine(Path.GetTempPath(), "tftape_vod_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        return dir;
    }

    private sealed class FakeVodLibraryService : IVodLibraryService
    {
        public IReadOnlyList<VodEntry> Entries { get; set; } = Array.Empty<VodEntry>();

        public Task<IReadOnlyList<VodEntry>> ScanAsync(string folder, CancellationToken ct = default) =>
            Task.FromResult(Entries);
    }
}
