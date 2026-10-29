import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import stat
import tempfile
import unittest
import zipfile
import zlib

spec = importlib.util.spec_from_file_location("prepare_assets", Path(__file__).with_name("prepare_assets.py"))
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


class PrepareAssetsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="zg-ios-assets-test-")
        self.root = Path(self.temporary.name).resolve()

    def tearDown(self):
        self.temporary.cleanup()

    def apk(self, *, malformed=None, extra=None):
        files = {"client.js": b"old packed client", "native107/rabbit.glb": b"glTF" + b"a" * 400000,
                 "textures/pixel.png": b"\x89PNG\r\n\x1a\n", "tables/test.json": b'{"value":7}'}
        pack, index = bytearray(), {}
        for i, (name, decoded) in enumerate(files.items()):
            stored = decoded if i == 2 else zlib.compress(decoded) if i == 3 else gzip.compress(decoded, mtime=0)
            index[name] = [len(pack), len(stored), 0 if i == 2 else 1]
            pack.extend(stored)
        if malformed:
            malformed(pack, index)
        path = self.root / "fixture.apk"
        with zipfile.ZipFile(path, "w") as apk:
            apk.writestr("assets/index.html", "<script type=module src=client.js></script>")
            apk.writestr("assets/client.js", "export const current='direct-r119';")
            apk.writestr("assets/resources107.pak", pack)
            apk.writestr("assets/resources107-index.json", json.dumps(index))
            if extra:
                extra(apk)
        return path, hashlib.sha256(path.read_bytes()).hexdigest(), files

    def test_stream_all_codecs_and_source_precedence(self):
        apk, sha, originals = self.apk()
        destination = self.root / "GameAssets"
        result = assets.prepare(apk, destination, expected_sha256=sha)
        manifest = result["manifest"]
        self.assertEqual(result["status"], "prepared")
        self.assertEqual(manifest["summary"]["overriddenPackedCount"], 1)
        self.assertEqual(set(manifest["summary"]["codecs"]), {"0", "1", "2"})
        with (destination / "files/resources107.pak").open("rb") as stream:
            for name, row in manifest["packed"].items():
                offset, length, size, codec, digest = row
                content = b"".join(assets.indexed_payload(stream, offset, length, codec))
                self.assertEqual(content, originals[name])
                self.assertEqual(len(content), size)
                self.assertEqual(hashlib.sha256(content).hexdigest(), digest)
        self.assertIn(b"direct-r119", (destination / "files/client.js").read_bytes())
        before = {p.relative_to(destination).as_posix(): p.stat().st_mtime_ns for p in destination.rglob("*") if p.is_file()}
        again = assets.prepare(apk, destination, expected_sha256=sha)
        self.assertEqual(again["status"], "verified-existing")
        after = {p.relative_to(destination).as_posix(): p.stat().st_mtime_ns for p in destination.rglob("*") if p.is_file()}
        self.assertEqual(before, after, "idempotent run cannot rewrite even identical managed assets")
        self.assertEqual(sha, assets.sha256_file(apk), "source APK unchanged")

    def test_source_mismatch_before_any_output(self):
        apk, _, _ = self.apk(); destination = self.root / "new-parent/GameAssets"
        with self.assertRaisesRegex(assets.AssetError, "SHA256"):
            assets.prepare(apk, destination)
        self.assertFalse(destination.parent.exists())

    def test_unowned_existing_directory_is_untouched(self):
        apk, sha, _ = self.apk(); destination = self.root / "external"
        destination.mkdir(); important = destination / "keep.txt"; important.write_text("external")
        with self.assertRaisesRegex(assets.AssetError, "refusing to overwrite"):
            assets.prepare(apk, destination, expected_sha256=sha)
        self.assertEqual(important.read_text(), "external")

    def test_changed_or_unmanaged_output_is_not_repaired_by_overwrite(self):
        apk, sha, _ = self.apk(); destination = self.root / "GameAssets"
        assets.prepare(apk, destination, expected_sha256=sha)
        client = destination / "files/client.js"; client.write_text("user edit")
        with self.assertRaisesRegex(assets.AssetError, "refusing to overwrite"):
            assets.prepare(apk, destination, expected_sha256=sha)
        self.assertEqual(client.read_text(), "user edit")
        destination2 = self.root / "OtherAssets"; assets.prepare(apk, destination2, expected_sha256=sha)
        extra = destination2 / "notes.txt"; extra.write_text("keep me")
        with self.assertRaisesRegex(assets.AssetError, "Unmanaged"):
            assets.prepare(apk, destination2, expected_sha256=sha)
        self.assertEqual(extra.read_text(), "keep me")

    def test_path_bounds_symlink_and_pack_bounds_fail_closed(self):
        for number, name in enumerate(["../escape.js", "C:/escape.js", "a\\b.js", "a//b.js", "a/./b.js", "a/../b.js"]):
            with self.subTest(name=name):
                def raw_name(z):
                    record = zipfile.ZipInfo("placeholder")
                    record.filename = "assets/" + name
                    z.writestr(record, b"unsafe")
                apk, sha, _ = self.apk(extra=raw_name)
                dest = self.root / ("invalid-" + str(number))
                with self.assertRaises(assets.AssetError): assets.prepare(apk, dest, expected_sha256=sha)
                self.assertFalse(dest.exists())
        self.assertFalse((self.root / "escape.js").exists())
        def symlink(z):
            record = zipfile.ZipInfo("assets/link.js"); record.external_attr = (stat.S_IFLNK | 0o777) << 16
            z.writestr(record, "../../outside")
        apk, sha, _ = self.apk(extra=symlink)
        with self.assertRaisesRegex(assets.AssetError, "symlink"):
            assets.prepare(apk, self.root / "symlink", expected_sha256=sha)
        for bad in [[-1, 10, 0], [0, 0x7fffffff, 1], [0, 1, 2]]:
            apk, sha, _ = self.apk(malformed=lambda p, i: i.update({"bad.bin": bad}))
            with self.assertRaisesRegex(assets.AssetError, "Out-of-bounds"):
                assets.prepare(apk, self.root / "bad-index", expected_sha256=sha)

    def test_truncated_and_trailing_compressed_payloads(self):
        def truncated(pack, index): index["native107/rabbit.glb"][1] -= 1
        apk, sha, _ = self.apk(malformed=truncated)
        with self.assertRaisesRegex(assets.AssetError, "complete end/CRC"):
            assets.prepare(apk, self.root / "truncated", expected_sha256=sha)
        def trailing(pack, index): index["client.js"][1] += 1
        apk, sha, _ = self.apk(malformed=trailing)
        with self.assertRaisesRegex(assets.AssetError, "Trailing"):
            assets.prepare(apk, self.root / "trailing", expected_sha256=sha)
        self.assertFalse(list(self.root.glob(".*.prepare-*")), "only fresh staging is cleaned on rejection")


if __name__ == "__main__":
    unittest.main()
