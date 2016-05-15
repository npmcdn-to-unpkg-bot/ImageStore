package cz.jurankovi.imgserver.client;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import javax.ws.rs.core.Response;

import org.jboss.resteasy.client.jaxrs.ResteasyClient;
import org.jboss.resteasy.client.jaxrs.ResteasyClientBuilder;
import org.jboss.resteasy.client.jaxrs.ResteasyWebTarget;

import cz.jurankovi.imgserver.model.rest.Image;
import cz.jurankovi.imgserver.rest.ImageResource;

public class SimpleImgClient {
    
    private static final int BUFFER_SIZE = 1024;
    private static final String HASH_ALG = "SHA-256";

    public static void main(String[] args) throws Exception {
        if (args.length != 1) {
            System.err.println("Exactly one parameter (path to image) required, got " + args.length);
            System.exit(1);
        }
        String imgPath = args[0];
        File file = new File(imgPath);
        String imgSha256 = digestToString(sha256(file));
        Image img = new Image(imgPath, imgSha256);
        
        ResteasyClient client = new ResteasyClientBuilder().build();
        ResteasyWebTarget target = (ResteasyWebTarget) client.target("http://localhost:8080/imgserver/rest");
        ImageResource imgRes = target.proxy(ImageResource.class);
        
        Response res = imgRes.prepareUpload(null, img);
        if (Response.Status.OK != res.getStatusInfo()) {
            //TODO hande failure
        }
        URI uploadURI = res.getLink("upload").getUri();
        System.out.println("upload to " + uploadURI);
      //TODO really use URI, it can be different server than one with DB
        Long imgId = Long.valueOf(res.getHeaderString("imgId"));
        res.close();
        
        
        try (InputStream is = new FileInputStream(file)) {
            res = imgRes.upload(imgId, is);
            if (Response.Status.OK != res.getStatusInfo()) {
                //TODO hande failure
            }
        } finally {
            res.close();
        }
    }
    
    private static byte[] sha256(File f) throws IOException, NoSuchAlgorithmException {
        MessageDigest md = MessageDigest.getInstance(HASH_ALG);
        try (FileInputStream fis = new FileInputStream(f)) {
            byte[] buff = new byte[BUFFER_SIZE];
            int len = 0;
            while ((len = fis.read(buff)) != -1) {
                md.update(buff, 0, len);
            }
        }
        return md.digest();
    }
    
    private static String digestToString(byte[] digest) {
        StringBuffer sb = new StringBuffer();
        for (byte b : digest) {
            sb.append(String.format("%02x", b));
        }
        return sb.toString();
    }
}
