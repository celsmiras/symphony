package symphony.bm.bm_comms;

import com.mongodb.*;
import org.json.JSONException;
import org.json.JSONObject;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import symphony.bm.bm_comms.jeep.exceptions.PrimaryMessageCheckingException;
import symphony.bm.bm_comms.jeep.vo.*;
import symphony.bm.bm_comms.mongodb.BMCommsMongoDBManager;
import symphony.bm.bm_comms.rest.RestMicroserviceCommunicator;

import java.net.UnknownHostException;
import java.util.LinkedList;

public class InboundTrafficManager implements Runnable {
    private final Logger LOG;
    private LinkedList<RawMessage> rawMsgQueue = new LinkedList<RawMessage>();
//    private ResponseManager rm;
    private RestMicroserviceCommunicator rest;
    private BMCommsMongoDBManager mongoDBManager;
    private String devicesDBCollection;

    public InboundTrafficManager(String logDomain, String logName, RestMicroserviceCommunicator rest/*,
                                 ResponseManager responseManager*/, BMCommsMongoDBManager mongoDBManager,
                                 String devicesDBCollection) {
        LOG = LoggerFactory.getLogger(logDomain + "." + logName);
//        this.rm = responseManager;
        this.rest = rest;
        this.mongoDBManager = mongoDBManager;
        this.devicesDBCollection = devicesDBCollection;

        Thread t = new Thread(this,logDomain + "." + logName);
        t.start();
        LOG.info(InboundTrafficManager.class.getSimpleName() + " started!");
    }

    public void addInboundRawMessage(RawMessage rawMsg) {
        rawMsgQueue.add(rawMsg);
    }

    @Override
    public void run() {
        while(!Thread.currentThread().isInterrupted()) {
            RawMessage rawMsg = rawMsgQueue.poll();
            if(rawMsg != null) {
                LOG.debug("New JEEP message received! Checking primary message validity...");
                try {
                    if (checkPrimaryMessageValidity(rawMsg) == JeepMessageType.REQUEST) {
                        LOG.debug("JEEP request found! Forwarding to logic layer...");
                        JeepRequest request = new JeepRequest(new JSONObject(rawMsg.getMessageStr()),
                                rawMsg.getProtocol());
                        rest.forwardJeepMessage(request);
                    } else if (checkPrimaryMessageValidity(rawMsg) == JeepMessageType.RESPONSE) {
                        LOG.debug("JEEP response found! Forwarding to logic layer...");
                        JeepResponse response = new JeepResponse(new JSONObject(rawMsg.getMessageStr()),
                                rawMsg.getProtocol());
                        rest.forwardJeepMessage(response);
                    }
                } catch(PrimaryMessageCheckingException e) {
                    sendError(e.getMessage(), rawMsg.getProtocol());
                }
            }
        }
    }

    /**
     * Checks if the raw JEEP message string contains all the required primary parameters
     *
     * @param rawMsg The RawMessage object
     * @return The JEEP message type of the request if valid; either <b><i>JeepMessageType.REQUEST</b></i>
     * 		or <b><i>JeepMessageType.RESPONSE</i></b>, <b><i>null</i></b> if: <br>
     * 		<ul>
     * 			<li>The intercepted request is not in JSON format</li>
     * 			<li>There are missing primary request parameters</li>
     * 			<li>There are primary request parameters that are null/empty</li>
     * 			<li>CID does not exist</li>
     * 			<li>MSN does not exist</li>
     * 		</ul>
     */
    private JeepMessageType checkPrimaryMessageValidity(RawMessage rawMsg)
            throws PrimaryMessageCheckingException {
        LOG.trace("Checking primary request parameters...");
        JSONObject json;
        String request = rawMsg.getMessageStr();

        //#1: Checks if the intercepted request is in proper JSON format
        try {
            json = new JSONObject(request);
        } catch(JSONException e) {
            throw new PrimaryMessageCheckingException("Improper JSON construction!");
        }

        //#2: Checks if there are missing primary request parameters
        if(!json.keySet().contains("MRN") || !json.keySet().contains("CID") ||
                !json.keySet().contains("MSN")) {
            throw new PrimaryMessageCheckingException("Request does not contain all primary " +
                    "request parameters!");
        }

        //#3: Checks if the primary request parameters are null/empty
        if(json.getString("MRN").equals("") || json.getString("MRN") == null) {
            throw new PrimaryMessageCheckingException("Null MRN!");
        } else if(json.getString("CID").equals("") || json.getString("CID") == null) {
            throw new PrimaryMessageCheckingException("Null CID!");
        } else if(json.getString("MSN").equals("") || json.getString("MSN") == null) {
            throw new PrimaryMessageCheckingException("Null MSN!");
        }

        //#4: Checks if CID exists
        if(json.getString("MSN").equals("register") ||
                (json.getString("MSN").equals("getRooms") &&
                        json.getString("CID").equals("default_topic")));
        else if(!checkIfDeviceExists(json.getString("CID"))) {
            throw new PrimaryMessageCheckingException("CID does not exist!");
        }

        //#5 Checks if MSN exists
        boolean b = false;

        if(rest.checkMSN(json.getString("MSN"))) {
            b = true;
        }

        if(b) {
            LOG.trace("Checking if message is a request or response...");
            if(json.has("success")) {
                LOG.trace("Primary message parameters good to go!");
                return JeepMessageType.RESPONSE;
            } else {
                LOG.trace("Primary message parameters good to go!");
                return JeepMessageType.REQUEST;
            }
        }
        else {
            throw new PrimaryMessageCheckingException("Invalid MSN!");
        }
    }

    private boolean checkIfDeviceExists(String cid) {
        DBObject query = new BasicDBObject("CID", cid);
        DBCursor cursor = mongoDBManager.query(devicesDBCollection, query);
        return cursor.length() > 0;
    }

    private void sendError(String message, Protocol protocol) {
        LOG.error(message);
        protocol.getSender().sendErrorResponse(new JeepErrorResponse(message, protocol));
    }
}
