import importlib.util
import unittest
from pathlib import Path


class SchemeCANIPSITest(unittest.TestCase):
	def test_basic_complete_and_experiment_flows(self:object) -> None:
		repository_directory = Path(__file__).resolve().parents[2]
		target = repository_directory / "SchemeCANIFPPCT" / "SchemeCANIPSI.py"
		self.assertTrue(target.is_file(), "SchemeCANIPSI.py must exist. ")
		specification = importlib.util.spec_from_file_location("scheme_canipsi", target)
		self.assertIsNotNone(specification)
		self.assertIsNotNone(specification.loader)
		module = importlib.util.module_from_spec(specification)
		specification.loader.exec_module(module)
		if module.PairingGroup is None:
			self.skipTest("Charm-Crypto is unavailable in this interpreter. ")

		group = module.PairingGroup("SS512", secparam = 128)
		scheme = module.SchemeCANIPSI(group)
		keyword = b"shared-keyword"
		other_keyword = b"different-keyword"
		secrets = tuple(group.random(module.ZR) for _ in range(3))
		identity = group.random(module.ZR)

		scheme.BSetup(3, 1)
		self.assertFalse(scheme.BQuery(None, None))
		basic_user_key = scheme.BKGen(identity)
		basic_ciphertext = scheme.BEncryption(keyword, secrets, secrets[0])
		self.assertTrue(scheme.BQuery(basic_ciphertext, scheme.BTokenGen(keyword, basic_user_key)))
		self.assertFalse(scheme.BQuery(basic_ciphertext, scheme.BTokenGen(other_keyword, basic_user_key)))

		tracing_list = []
		scheme.Setup(3, 1)
		self.assertFalse(scheme.Query(None, None, secrets))
		self.assertFalse(scheme.Trace(None, []))
		secret_key, encryption_key = scheme.KGen(identity, tracing_list)
		ciphertext = scheme.Encryption(keyword, secret_key, encryption_key, secrets, secrets[0])
		self.assertTrue(scheme.Query(ciphertext, scheme.TokenGen(keyword, secret_key), secrets))
		self.assertFalse(scheme.Query(ciphertext, scheme.TokenGen(other_keyword, secret_key), secrets))
		self.assertEqual(identity, scheme.Trace(ciphertext, tracing_list)[0])

		result = module.conductScheme(("SS512", 128), n = 3, m = 1, run = 1, isVerbose = False)
		self.assertTrue(result[5])
		self.assertTrue(result[6])
		self.assertTrue(result[7])
		self.assertTrue(result[8])


if "__main__" == __name__:
	unittest.main()
