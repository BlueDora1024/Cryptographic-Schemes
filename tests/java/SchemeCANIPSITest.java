import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

import it.unisa.dia.gas.jpbc.Element;

public final class SchemeCANIPSITest
{
	private static void require(final boolean condition, final String message)
	{
		if (!condition)
			throw new AssertionError(message);
	}

	public static void main(final String[] arguments)
	{
		final SchemeCANIPSI scheme = new SchemeCANIPSI(new SchemeCANIPSI.CurveParameter("SS512", 128));
		final byte[] keyword = "shared-keyword".getBytes(StandardCharsets.UTF_8);
		final byte[] otherKeyword = "different-keyword".getBytes(StandardCharsets.UTF_8);
		final List<Element> secrets = scheme.randomSecrets(3);
		final Element identity = scheme.randomSecrets(1).get(0);

		scheme.BSetup(3, 1);
		require(!scheme.BQuery(null, null), "A malformed basic query must fail.");
		final Element basicUserKey = scheme.BKGen(identity);
		final SchemeCANIPSI.BasicCipherText basicCipherText = scheme.BEncryption(keyword, secrets, secrets.get(0));
		require(scheme.BQuery(basicCipherText, scheme.BTokenGen(keyword, basicUserKey)), "The matching basic query must succeed.");
		require(!scheme.BQuery(basicCipherText, scheme.BTokenGen(otherKeyword, basicUserKey)), "The mismatching basic query must fail.");

		final List<SchemeCANIPSI.TraceEntry> tracingList = new ArrayList<>();
		scheme.Setup(3, 1);
		require(!scheme.Query(null, null, secrets), "A malformed complete query must fail.");
		require(scheme.Trace(null, new ArrayList<>()) == null, "Malformed tracing input must not produce an identity.");
		final SchemeCANIPSI.UserKeys userKeys = scheme.KGen(identity, tracingList);
		final SchemeCANIPSI.CipherText cipherText = scheme.Encryption(keyword, userKeys.secretKey(), userKeys.encryptionKey(), secrets, secrets.get(0));
		require(scheme.Query(cipherText, scheme.TokenGen(keyword, userKeys.secretKey()), secrets), "The matching complete query must succeed.");
		require(!scheme.Query(cipherText, scheme.TokenGen(otherKeyword, userKeys.secretKey()), secrets), "The mismatching complete query must fail.");
		final SchemeCANIPSI.TraceEntry traced = scheme.Trace(cipherText, tracingList);
		require(traced != null && identity.isEqual((Element)traced.identity()), "Trace must recover the registered identity.");

		final SchemeCANIPSI.RunResult result = SchemeCANIPSI.conductScheme(new SchemeCANIPSI.CurveParameter("SS512", 128), 3, 1, Integer.valueOf(1), false);
		require(result.systemValid(), "conductScheme must create a supported system.");
		require(Boolean.TRUE.equals(result.basicSchemeCorrect()), "conductScheme must validate the basic flow.");
		require(result.schemeCorrect(), "conductScheme must validate the complete flow.");
		require(result.tracingVerified(), "conductScheme must validate tracing.");
		System.out.println("SchemeCANIPSI Java tests passed.");
	}
}
