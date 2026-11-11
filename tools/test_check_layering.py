"""Regression checks for the SDK assembly/shared-kernel boundary."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import check_layering as layering


class BootstrapLayeringTest(unittest.TestCase):
    def test_composition_root_can_wire_modules_and_os_adapters(self):
        root = layering.classify(Path("src/bootstrap/composition_root.cpp"))
        for target in ("src/modules/crypto/infrastructure/provider.h",
                       "src/core/infrastructure/logger.h", "src/platform/macos/vault.h"):
            with self.subTest(target=target):
                self.assertIsNone(layering.check_edge(root, layering.classify(Path(target))))

    def test_inner_layers_cannot_include_bootstrap(self):
        destination = layering.classify(Path("src/bootstrap/composition_root.h"))
        for source in ("src/core/domain/value.h", "src/core/application/sdk.h",
                       "src/core/infrastructure/logger.cpp", "src/modules/wallet/api/wallet.h",
                       "src/modules/wallet/application/create.cpp", "src/platform/macos/vault.cpp"):
            with self.subTest(source=source):
                self.assertIsNotNone(layering.check_edge(layering.classify(Path(source)), destination))

    def test_bindings_can_enter_bootstrap(self):
        self.assertIsNone(layering.check_edge(
            layering.classify(Path("src/bindings/c_api/sdk_c_api.cpp")),
            layering.classify(Path("src/bootstrap/composition_root.h"))))

    def test_core_has_no_composition_root_exception(self):
        self.assertEqual(layering.classify(Path("src/core/composition_root.cpp")).layer, "misplaced")
        self.assertIsNotNone(layering.check_edge(
            layering.classify(Path("src/core/infrastructure/logger.cpp")),
            layering.classify(Path("src/modules/crypto/api/crypto.h"))))

    def test_platform_cannot_include_modules(self):
        destination = layering.classify(Path("src/modules/network/application/ports/connector.h"))
        for source in ("src/platform/tcp_socket.h", "src/platform/linux/tcp_socket.cpp"):
            with self.subTest(source=source):
                self.assertEqual(
                    layering.check_edge(layering.classify(Path(source)), destination),
                    "platform must not depend on modules, bindings or bootstrap")

    def test_scanner_rejects_old_platform_include(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "src/platform/tcp_socket.h"
            destination = root / "src/modules/network/application/ports/i_node_connector.h"
            source.parent.mkdir(parents=True)
            destination.parent.mkdir(parents=True)
            destination.write_text("#pragma once\n")
            source.write_text('#include "modules/network/application/ports/i_node_connector.h"\n')
            result = subprocess.run(
                [sys.executable, str(Path(layering.__file__)), "--root", directory],
                capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 1)
            self.assertIn("platform must not depend on modules", result.stdout)

    def test_scanner_rejects_real_include_violation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "src/core/application/bad.cpp"
            destination = root / "src/bootstrap/composition_root.h"
            source.parent.mkdir(parents=True)
            destination.parent.mkdir(parents=True)
            destination.write_text("#pragma once\n")
            source.write_text('#include "bootstrap/composition_root.h"\n')
            result = subprocess.run(
                [sys.executable, str(Path(layering.__file__)), "--root", directory],
                capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 1)
            self.assertIn("must not depend on bootstrap", result.stdout)


if __name__ == "__main__":
    unittest.main()
