package symphony.bm.comms.mqtt;

import org.apache.http.HttpResponse;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.*;
import org.apache.http.entity.ContentType;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.HttpClientBuilder;
import org.apache.http.util.EntityUtils;
import org.eclipse.paho.client.mqttv3.*;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;
import symphony.bm.comms.rest.ServiceLocator;

import java.io.IOException;
import java.util.List;
import java.util.Queue;
import java.util.Vector;
import java.util.concurrent.LinkedBlockingQueue;

@Component
public class MQTTManager implements MqttCallback, Runnable {
    private final Logger LOG = LoggerFactory.getLogger(MQTTManager.class);
    private MqttClient mqttClient;
    private Queue<MqttMessage> messageQueue = new LinkedBlockingQueue<>();
    
    private final int qos;
    private final String serverURI, clientID, bmTopic, univTopic, devicesTopic, registerTopic, errorTopic;
    private final String registerMSN;
    private final List<ServiceLocator> serviceLocators;
    
    private int messages = 0;
    
    public MQTTManager(@Value("${mqtt.serverURI}") String serverURI, @Value("${mqtt.clientID}") String clientID,
                       @Value("${mqtt.topic.bm}") String bmTopic, @Value("${mqtt.topic.universal}") String univTopic,
                       @Value("${mqtt.topic.devices}") String devicesTopic,
                       @Value("${mqtt.topic.error}") String errorTopic,
                       @Value("${mqtt.topic.bm.register}") String registerTopic,
                       @Value("${mqtt.qos}") int qos,
                       @Value("${services.register.msn}") String registerMSN,
                       @Qualifier("serviceLocators") List<ServiceLocator> serviceLocators) {
        this.serverURI = serverURI;
        this.clientID = clientID;
        this.bmTopic = bmTopic;
        this.univTopic = univTopic;
        this.devicesTopic = devicesTopic;
        this.registerTopic = registerTopic;
        this.errorTopic = errorTopic;
        this.qos = qos;
        this.registerMSN = registerMSN;
        this.serviceLocators = serviceLocators;
        
        connectToMQTT();
    }
    
    private boolean connectToMQTT() {
        try {
            LOG.info("Connecting to MQTT @ " + serverURI + "...");
            this.mqttClient = new MqttClient(serverURI, clientID, new MemoryPersistence());
//            JSONObject lastWill = new JSONObject();
//            lastWill.put("RID", "BM-exit");
//            lastWill.put("RTY", "exit");
//            lastWill.put("msg", "BM has terminated.");
            MqttConnectOptions connOpts = new MqttConnectOptions();
//            connOpts.setWill(default_topic, lastWill.toString().getBytes(), 2, false);
            connOpts.setCleanSession(true);
            mqttClient.connect(connOpts);
            LOG.info("Connected to MQTT!");
            mqttClient.subscribe(bmTopic);
            LOG.debug("Subscribed to BM topic!");
            mqttClient.setCallback(this);
            LOG.debug("Listener set!");
            LOG.debug("Publisher set!");
            return true;
        } catch (MqttSecurityException e) {
            LOG.error("Cannot connect to MQTT server due to MqttSecurityException!", e);
            LOG.info("Attempting to reconnect...");
            connectToMQTT();
            return false;
        } catch (MqttException e) {
            LOG.error("Cannot connect to MQTT due to MqttException!", e);
            LOG.info("Attempting to reconnect...");
            connectToMQTT();
            return false;
        }
    }
    
    @Override
    public void connectionLost(Throwable throwable) {
        LOG.error("Connection lost to MQTT", throwable);
    }
    
    @Override
    public void messageArrived(String s, MqttMessage mqttMessage) throws Exception {
        messages++;
        LOG.info("Message arrived at topic " + s);
        LOG.info("Message: " + mqttMessage.toString());
        messageQueue.offer(mqttMessage);
        Thread t = new Thread(this, "Message " + messages);
        t.start();
    }
    
    @Override
    public void deliveryComplete(IMqttDeliveryToken iMqttDeliveryToken) {
    
    }
    
    @Override
    public void run() {
        MqttMessage mqttMessage = messageQueue.poll();
        HttpClient httpClient = HttpClientBuilder.create().build();
        HttpUriRequest httpMessage;
        JSONObject jsonReq;
        ServiceLocator locator;
        try {
            jsonReq = new JSONObject(mqttMessage.toString());
        } catch (JSONException e) {
            String errorMsg = "Unable to parse JSON from message string";
            LOG.error(errorMsg, e);
            try {
                publishToErrorTopic(errorMsg);
            } catch (MqttException e1) {
                LOG.error("Unable to publish to error topic", e1);
            }
            throw e;
        }
        try {
            locator = getServiceLocator(jsonReq.getString("MSN"));
        } catch (JSONException e) {
            String errorMsg = "No MSN field found";
            LOG.error(errorMsg, e);
            try {
                publishToErrorTopic(errorMsg);
            } catch (MqttException e1) {
                LOG.error("Unable to publish to error topic", e1);
            }
            throw e;
        }
        if (locator == null) {
            String errorMsg = "Invalid MSN";
            LOG.error(errorMsg);
            try {
                publishToErrorTopic(errorMsg);
            } catch (MqttException e1) {
                LOG.error("Unable to publish to error topic", e1);
            }
            return;
        }
    
        String uri = locator.getServiceURL();
        for (String var : locator.getVariablePaths()) {
            uri += "/" + jsonReq.getString(var);
        }
        switch (locator.getHttpMethod()) {
            case DELETE:
                httpMessage = new HttpDelete(uri);
                break;
            case GET:
                httpMessage = new HttpGet(uri);
                break;
            case HEAD:
                httpMessage = new HttpHead(uri);
                break;
            case OPTIONS:
                httpMessage = new HttpOptions(uri);
                break;
            case PATCH:
                HttpPatch patch = new HttpPatch(uri);
                patch.setEntity(new StringEntity(mqttMessage.toString(), ContentType.APPLICATION_JSON));
                httpMessage = patch;
                break;
            case PUT:
                HttpPut put = new HttpPut(uri);
                put.setEntity(new StringEntity(mqttMessage.toString(), ContentType.APPLICATION_JSON));
                httpMessage = put;
                break;
            case POST:
                HttpPost post = new HttpPost(uri);
                post.setEntity(new StringEntity(mqttMessage.toString(), ContentType.APPLICATION_JSON));
                httpMessage = post;
                break;
            default: // TRACE
                httpMessage = new HttpTrace(locator.getBmURL() + ":" + locator.getPort() + "/" + locator.getPath());
        }
    
        try {
            HttpResponse response = httpClient.execute(httpMessage);
            List<JSONObject> rspJsons = new Vector<>();
            String rspStr = EntityUtils.toString(response.getEntity());
            LOG.info("Response from service: " + rspStr);
            try {
                JSONArray rspJsonArray = new JSONArray(rspStr);
                for (int i = 0; i < rspJsonArray.length(); i++) {
                    rspJsons.add(rspJsonArray.getJSONObject(i));
                }
            } catch (JSONException e) {
                rspJsons.add(new JSONObject(rspStr));
            }
    
            for (JSONObject jsonRsp : rspJsons) {
                String cid;
                try { // message is a JEEP request
                    cid = jsonRsp.getString("CID");
                } catch (JSONException e) { // message is a JEEP response
                    cid = jsonReq.getString("CID");
                    jsonRsp.put("MRN", jsonReq.get("MRN"));
                }
                try {
                    if (jsonReq.getString("MSN").equals(registerMSN)) {
                        publishToRegisterTopic(jsonRsp.toString());
                    }
                    publishToDevice(cid, jsonRsp.toString());
                } catch (MqttException e) {
                    LOG.error("Unable to publish to device " + cid, e);
                }
            }
        } catch (IOException e) {
            LOG.error("IOException in message forwarding", e);
        }
    }
    
    private void publishToDevice(String cid, String msg) throws MqttException {
        String topic = devicesTopic + "/" + cid;
        publish(topic, msg.getBytes(), qos, false);
    }
    
    private void publishToRegisterTopic(String msg) throws MqttException {
        String topic = univTopic + "/" + registerTopic;
        publish(topic, msg.getBytes(), qos, false);
    }
    
    private void publishToErrorTopic(String msg) throws MqttException {
        String topic = univTopic + "/" + errorTopic;
        publish(topic, msg.getBytes(), qos, false);
    }
    
    private void publish(String topic, byte[] payload, int qos, boolean retained) throws MqttException {
        LOG.info("Publishing to " + topic + " topic");
        LOG.info("Message: " + payload.toString());
        mqttClient.publish(topic, payload, qos, false);
        mqttClient.publish(univTopic, payload, qos, false);
    }
    
    private ServiceLocator getServiceLocator(String msn) {
        for (ServiceLocator sl : serviceLocators) {
            if (sl.getMSN().equalsIgnoreCase(msn)) {
                return sl;
            }
        }
        return null;
    }
}
